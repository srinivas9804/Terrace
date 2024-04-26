#ifndef TERRACE_GRAPH
#define TERRACE_GRAPH
#define NO_TLX

#include <vector>
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <set>
#include <filesystem>
#include <sstream>

#include "PMA/PCSR.hpp"
#include "cpam/cpam.h"
#include "parlay/parallel.h"

#define IN_PLACE 13
#define PMA_SIZE 1024

class TerraceGraph{
private:
        struct edge_entry {
                using key_t = uint32_t;  // a vertex_id
                static inline bool comp(key_t a, key_t b) { return a < b; }
        };
        
        typedef struct __attribute__ ((__packed__)) {
                uint32_t degree; // 4 Bytes
                std::array<uint32_t,IN_PLACE> neighbors; // 4*13=52 Bytes
        #if WEIGHTED
                uint32_t weights[NUM_IN_PLACE_NEIGHBORS]; // 4*14*2=112 Bytes
                uint32_t padding; // 4 Bytes padding (112+4+4+8=128)
        #endif
                cpam::diff_encoded_set<edge_entry, 64>* cpam_neighbors{nullptr};  // 8 Bytes
        } level1_block;

        size_t nodes;
        size_t edges;
        level1_block* l1_block;
        PCSR<simple_pcsr_settings<uint32_t>> cpma;
public:
        static std::conditional<false, void*, void*> short_mode;
        static const bool support_insert_batch = false;
        struct weight_type{};
        size_t N() const {return nodes;}
        size_t M() const {return edges;}
        auto out_degree(size_t i) const { return l1_block[i].degree; }
        auto in_degree(size_t i) const { return l1_block[i].degree; }

        void build_batch(std::vector<std::tuple<int,int,int>> edgesStruct, bool sorted = false){
                if(!sorted){
                        std::sort(edgesStruct.begin(),edgesStruct.end(),[](const std::tuple<int,int,int> &a, const std::tuple<int,int,int> &b)
                        {
                                if(std::get<0>(a) != std::get<0>(b)) 
                                        return std::get<0>(a) < std::get<0>(b); 
                                return std::get<1>(a) < std::get<1>(b);
                        });
                }
                edges = edgesStruct.size();
                int lastSrc = std::get<0>(edgesStruct.back());
                increaseSize(lastSrc+1);
                std::vector<std::atomic<uint32_t>> degree(lastSrc+1);
                parlay::parallel_for(0,edgesStruct.size(),[&](size_t i){
                        int src = std::get<0>(edgesStruct[i]);
                        degree[src]++;
                });
                parlay::parallel_for(0,lastSrc+1,[&](size_t i){
                        l1_block[i].degree = degree[i];
                });
                // for(const auto &edge : edgesStruct){
                //         int src = std::get<0>(edge);
                //         l1_block[src].degree++;
                // }

                int startIndex = 0;
                int expectedPma = 0;
                std::vector<std::tuple<uint32_t,uint32_t>> pmaVector;
                for(size_t i=0;i<nodes;i++){
                        uint32_t deg = l1_block[i].degree;
                        if(deg > 0){
                                size_t j =0;
                                for(;j < deg && j < IN_PLACE;j++){
                                        auto [src,dest,weight] = edgesStruct[startIndex++];
                                        l1_block[src].neighbors[j] = dest;
                                }
                                if(deg > IN_PLACE){
                                        if(deg > PMA_SIZE + IN_PLACE){
                                                l1_block[i].cpam_neighbors = new cpam::diff_encoded_set<edge_entry, 64>();
                                                for(;j < deg;j++){
                                                        auto [src,dest,weight] = edgesStruct[startIndex++];
                                                        l1_block[src].cpam_neighbors->insert(dest);
                                                } 
                                        }
                                        else{
                                                for(;j < deg;j++){
                                                        auto [src,dest,weight] = edgesStruct[startIndex++];
                                                        std::tuple<uint64_t,uint64_t> edge(src,dest);
                                                        pmaVector.push_back(edge);
                                                }
                                        }
                                }
                        }
                }
                std::cout << "PMAVector size = " << pmaVector.size() << "\n";
                std::cout << "inserting" << cpma.insert_batch(pmaVector,true) << std::endl;
                std::cout << "PCSR nodes = " << cpma.num_nodes() << "\n";
                std::cout << "PCSR edges = " << cpma.num_edges() << "\n";
                std::cout << "startIndex =" << startIndex << "\n";
        }

        void increaseSize(int nodeCount){
                if(nodeCount > nodes){
                        if(l1_block == nullptr)
                                l1_block = (level1_block*) malloc(nodeCount * sizeof(*l1_block));
                        else{
                                l1_block = (level1_block*) realloc(l1_block, nodeCount * sizeof(*l1_block));
                        }
                        if(!l1_block){
                                std::cerr << "Could not allocate memory to l1_block" << std::endl;
                                std::terminate();
                        }
                        for(int i = nodes; i < nodeCount; i++){
                                l1_block[i].degree = 0;
                                l1_block[i].cpam_neighbors = nullptr;
                        }
                        nodes = nodeCount;
                }
        }

        TerraceGraph(size_t n) : cpma(n){
                nodes = n;
                edges = 0;
                l1_block = (level1_block*) malloc(n * sizeof(*l1_block));
                for(int i=0; i < n; i++){
                        l1_block[i].degree = 0;
                        l1_block[i].cpam_neighbors = nullptr;
                }
        }

        TerraceGraph(std::string filename, bool isSymmetric) : cpma(0){
                std::ifstream inputFile;
                if(!std::filesystem::exists(filename)){
                        std::cerr << "File: " << filename << ", does not exist"; 
                }
                inputFile.open(filename);
                std::string line;
                int lineCount = 0;
                while(std::getline(inputFile,line)){
                        if(std::isalpha(line[0]))
                                continue;
                        std::stringstream ss(line);
                        if(lineCount++ == 0)
                                ss >> nodes;
                        else{
                                ss >> edges;
                                break;
                        }
                }
                std::vector<std::tuple<int,int,int>> edgesStruct;
                std::cout << nodes << "," << edges << std::endl;
                increaseSize(nodes);
                std::vector<int> nodeIndex(nodes);
                for(int i=0;i<(int)nodes;i++){
                        inputFile >> nodeIndex[i];
                }
                for(int i=0;i<(int)(nodes - 1);i++){
                        for(int j=nodeIndex[i]; j<nodeIndex[i+1];j++){
                                int v;
                                inputFile >> v;
                                edgesStruct.emplace_back(i,v,0);
                                if(isSymmetric){
                                        edgesStruct.emplace_back(v,i,0);
                                }
                        }
                }
                for(int j=nodeIndex[nodes-1];j<(int)edges;j++){
                        int v;
                        inputFile >> v;
                        edgesStruct.emplace_back(nodes-1,v,0);
                }
                build_batch(edgesStruct,true);
                inputFile.close();
        }

        template <class F1, class F2, class F3, class W> void map_neighbors (size_t i, F1 f1, F2 f2, F3 f3, W w) const {
                size_t j = 0;
                for(;j < IN_PLACE && j < l1_block[i].degree; j++){
                        f1(i,l1_block[i].neighbors[j],w);
                }
                if(l1_block[i].degree >= IN_PLACE){
                        if(l1_block[i].cpam_neighbors != nullptr){
                                l1_block[i].cpam_neighbors->iterate_seq(f3);
                        }
                        else{
                                cpma.map_neighbors(i,f2, nullptr, false);
                        }
                }
        }
};


#endif