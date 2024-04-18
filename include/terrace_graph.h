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

#define IN_PLACE 13
#define PMA_SIZE 1024

class TerraceGraph{
private:
        struct edge_entry {
                using key_t = uint32_t;  // a vertex_id
                static inline bool comp(key_t a, key_t b) { return a < b; }
        };
        size_t nodes;
        size_t edges;
        std::vector<int> level1;
        std::vector<int> outDegree;
        PCSR<simple_pcsr_settings<uint32_t>> cpma;
        std::vector<cpam::diff_encoded_set<edge_entry, 64>*> level3;
public:
        static std::conditional<false, void*, void*> short_mode;
        static const bool support_insert_batch = false;
        struct weight_type{};
        size_t N() const {return nodes;}
        size_t M() const {return edges;}
        auto out_degree(size_t i) const { return outDegree[(int)i]; }
        auto in_degree(size_t i) const { return outDegree[(int)i]; }

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
                for(const auto &edge : edgesStruct){
                        int src = std::get<0>(edge);
                        outDegree[src]++;
                }

                int startIndex = 0;
                int expectedPma = 0;
                std::vector<std::tuple<uint32_t,uint32_t>> pmaVector;
                for(size_t i=0;i<nodes;i++){
                        if(outDegree[i] > 0){
                                size_t j =0;
                                for(;j < outDegree[i] && j < IN_PLACE;j++){
                                        auto [src,dest,weight] = edgesStruct[startIndex++];
                                        int index = src * IN_PLACE + j;
                                        if(index >= level1.size()){
                                                std::cout << "What is happening?" << std::endl;
                                        }
                                        level1[index] = dest;
                                }
                                if(outDegree[i] > IN_PLACE){
                                        if(outDegree[i] > PMA_SIZE + IN_PLACE){
                                                for(;j < outDegree[i];j++){
                                                        auto [src,dest,weight] = edgesStruct[startIndex++];
                                                        if(level3[src] == nullptr)
                                                                level3[src] = new cpam::diff_encoded_set<edge_entry, 64>();
                                                        level3[src]->insert(dest);
                                                } 
                                        }
                                        else{
                                                expectedPma += outDegree[i] - IN_PLACE;
                                                for(;j < outDegree[i];j++){
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
                        nodes = nodeCount;
                        level1.resize(IN_PLACE * nodes);
                        outDegree.resize(nodes);
                        level3.resize(nodes);
                }
        }

        TerraceGraph(size_t n) : cpma(n){
                nodes = n;
                edges = 0;
                level1.resize(IN_PLACE * n);
                level3.resize(n);
                outDegree.resize(n);
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
                outDegree.resize(nodes);
                level1.resize(nodes * IN_PLACE);
                level3.resize(nodes);
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
                if(outDegree[i] > 0){
                        size_t startIndex = IN_PLACE * i;
                        for(size_t j = 0;j < IN_PLACE && j < outDegree[(int)i]; j++){
                                if((startIndex + j) > level1.size()){
                                        std::cout << "Maybe error?" << std::endl;
                                }
                                f1(i,level1[startIndex + j],w);
                        }
                }
                if(outDegree[i] > IN_PLACE){
                        if(outDegree[i] > (IN_PLACE + PMA_SIZE)){
                                level3[i]->iterate_seq(f3);
                        }
                        else{
                                cpma.map_neighbors(i,f2, nullptr, false);
                        }
                }
        }
};


#endif