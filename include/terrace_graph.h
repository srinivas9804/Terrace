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

#include "absl/container/btree_set.h"
#include "absl/container/btree_map.h"

#include "PMA/PCSR.hpp"
#include "cpam/cpam.h"

#define IN_PLACE 13
#define PMA_SIZE 1024


namespace {
        template <class F, bool no_early_exit_, class W> struct F2 {
                F f;
                F2(F f_) : f(f_) {}
                W empty_weight = W();
                auto operator()(auto src, auto dest) { return f(src, dest, empty_weight); }
                static constexpr bool no_early_exit = no_early_exit_;
        };
};

class TerraceGraph{
private:
        size_t nodes;
        size_t edges;
        std::vector<int> level1;
        std::vector<int> outDegree;
        absl::btree_map<int,absl::btree_set<int>> level2;
        PCSR<simple_pcsr_settings<uint64_t>> cpma;
        std::vector<absl::btree_set<uint64_t>> level3;
public:
        static std::conditional<false, void*, void*> short_mode;
        static const bool support_insert_batch = false;
        struct weight_type{};
        size_t N() const {return nodes;}
        size_t M() const {return edges;}
        auto out_degree(size_t i) const { return outDegree[(int)i]; }
        auto in_degree(size_t i) const { return outDegree[(int)i]; }
    
        void add_edge(int u, int v, int w){
                if(outDegree[u] < IN_PLACE){
                        int index = u * IN_PLACE + outDegree[u];
                        level1[index] = v;
                }
                else if(outDegree[u] < IN_PLACE + PMA_SIZE){
                        level2[u].insert(v);
                }
                else{
                        level3[u].insert(v);
                }
                outDegree[u]++;
        }

        void build_batch(std::vector<std::tuple<int,int,int>> edgesStruct, bool sorted = false){
                if(!sorted){
                        std::sort(edgesStruct.begin(),edgesStruct.end(),[](const std::tuple<int,int,int> &a, const std::tuple<int,int,int> &b)
                        {
                                if(std::get<0>(a) != std::get<0>(b)) 
                                        return std::get<0>(a) < std::get<0>(b); 
                                return std::get<1>(a) < std::get<1>(b);
                        });
                        // for(auto elem : edges){
                        //         std::cout << std::get<0>(elem) << "," << std::get<1>(elem) << std::endl;
                        // }
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
                                                        level3[src].insert(dest);
                                                } 
                                        }
                                        else{
                                                // std::cout << "Do I enter here?" << std::endl;
                                                // std::cout << "outdegree[" << i << "]=" << outDegree[i] << std::endl;
                                                // std::cout << "j" << j << std::endl;
                                                expectedPma += outDegree[i] - IN_PLACE;
                                                for(;j < outDegree[i];j++){
                                                        // std::cout << "Do I enter here2?" << std::endl;
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
                std::cout << "PMA size = " << cpma.get_size() << "\n";
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

        void add_edges(std::vector<int> u, std::vector<int> v, std::vector<int> w){
                edges = u.size();
                for(auto i = 0; i < u.size();i++){
                        add_edge(u[i],v[i],0);
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
                std::cout << nodes << "," << edges << std::endl;
                outDegree.resize(nodes);
                level1.resize(nodes * IN_PLACE);
                level3.resize(nodes);
                std::vector<int> nodeIndex(nodes);
                for(int i=0;i<(int)nodes;i++){
                        inputFile >> nodeIndex[i];
                }
                for(int i=0;i<(int)(nodes - 1);i++){
                        // int count = nodeIndex[i+1] - nodeIndex[i];
                        // std::cout << "nodeIndex[" << i << "]=" << nodeIndex[i] << std::endl;
                        // std::cout << "nodeIndex[" << i+1 << "]=" << nodeIndex[i+1] << std::endl;
                        // std::cout << "neighbourCount[" << i << "]=" << count << std::endl; 
                        for(int j=nodeIndex[i]; j<nodeIndex[i+1];j++){
                                int v;
                                inputFile >> v;
                                add_edge(i,v,0);
                                if(isSymmetric){
                                        add_edge(v,i,0);
                                }
                        }
                }
                // std::cout << "neighbourCount[" << nodes-1 << "]=" << edges-nodeIndex[nodes-1] << std::endl; 
                for(int j=nodeIndex[nodes-1];j<(int)edges;j++){
                        int v;
                        inputFile >> v;
                        add_edge(nodes-1,v,0);
                }
                inputFile.close();
        }

        template <class F, class W> void map_neighbors (size_t i, F f) const {
                if(outDegree[i] > 0){
                        size_t startIndex = IN_PLACE * i;
                        for(size_t j = 0;j < IN_PLACE && j < outDegree[(int)i]; j++){
                                f(i,level1[startIndex + j],NULL);
                        }
                }
                if(outDegree[i] > IN_PLACE){
                        if(outDegree[i] > IN_PLACE + PMA_SIZE){
                                for(auto elem : level3[i]){
                                        f(i,elem,NULL);
                                }
                        }
                        else{
                                cpma.map_neighbors(i,F2<F, true, W>(f), nullptr, false);
                        }
                }
        }

        template <class F, class W> void map_neighbors (size_t i, F f, W w) const {
                // std::cout << "need to guess where error is?" << std::endl;
                if(outDegree[i] > 0){
                        size_t startIndex = IN_PLACE * i;
                        for(size_t j = 0;j < IN_PLACE && j < outDegree[(int)i]; j++){
                                if((startIndex + j) > level1.size()){
                                        std::cout << "Maybe error?" << std::endl;
                                }
                                f(i,level1[startIndex + j],w);
                        }
                }
                if(outDegree[i] > IN_PLACE){
                        if(outDegree[i] > IN_PLACE + PMA_SIZE){
                                for(auto elem : level3[i]){
                                        f(i,elem,w);
                                }
                        }
                        else{
                                cpma.map_neighbors(i,F2<F, true, W>(f), nullptr, false);
                        }
                }
        }
        
        template <class F> void parallel_map_out_neighbors (size_t i, F f) const{
                map_neighbors(i,f);
        }

        template <class F> void parallel_map_in_neighbors (size_t i, F f) const{
                map_neighbors(i,f);
        }

        template <class F> void map_out_neighbors (size_t i, F f) const{
                map_neighbors(i,f);
        }

        template <class F> void map_out_neighbors_early_exit (size_t i, F f) const{
                parallel_map_out_neighbors(i,f);
        }

        template <class F> void map_in_neighbors_early_exit (size_t i, F f) const{
                parallel_map_out_neighbors(i,f);
        }

        template <class F> void parallel_map_out_neighbors_early_exit (size_t i, F f, auto weight) const{
                parallel_map_out_neighbors(i,f);
        }

        template <class F> void parallel_map_in_neighbors_early_exit (size_t i, F f, auto weight) const{
                parallel_map_out_neighbors(i,f);
        }

        template <class F> size_t count_out_neighbors (size_t i, F f) const{
                size_t count = 0;
                int val = 0;
                if(outDegree[i] > 0){
                        size_t startIndex = IN_PLACE * i;
                        for(size_t j = 0;j < IN_PLACE && j < outDegree[(int)i]; j++){
                                count += f(i,level1[startIndex + j],NULL);
                        }
                }
                if(outDegree[i] > IN_PLACE){
                        for(auto& elem : level2.at(i)){
                                count += f(i, elem, NULL);
                        }
                }
                if(outDegree[i] > PMA_SIZE){
                        for(auto elem : level3[i]){
                                count += f(i, elem, NULL);
                        }
                }
                return count;
        }

        template <class F, class Monoid>
        auto map_reduce_neighbors(size_t id, F f, Monoid reduce) const {
                typename Monoid::T value = reduce.identity;
                map_neighbors(
                        id, [&](auto &...args) { value = reduce.f(value, f(args...)); });
                return value;
        }

        template <class F, class Monoid>
        auto map_reduce_in_neighbors(size_t id, F f, Monoid reduce) const {
                return map_reduce_neighbors(id,f,reduce);
        }

        template <class F, class Monoid>
        auto map_reduce_out_neighbors(size_t id, F f, Monoid reduce) const {
                return map_reduce_neighbors(id,f,reduce);
        }
};


#endif