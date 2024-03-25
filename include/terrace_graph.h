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

#define IN_PLACE 5
#define PMA_SIZE 5
class TerraceGraph{
private:
        size_t nodes;
        size_t edges;
        std::vector<std::array<int,IN_PLACE>> level1;
        std::vector<int> outDegree;
        absl::btree_map<int,absl::btree_set<int>> level2;
        std::vector<absl::btree_set<int>> level3;
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
                        level1[u][outDegree[u]] = v;
                }
                else if(outDegree[u] < IN_PLACE + PMA_SIZE){
                        level2[u].insert(v);
                }
                else{
                        level3[u].insert(v);
                }
                outDegree[u]++;
        }

        TerraceGraph(std::string filename, bool isSymmetric){
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
                level1.resize(nodes);
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

        template <class F> void map_neighbors (size_t i, F f) const {
                int val = 0;
                if(outDegree[i] > 0){
                        for(auto elem: level1){
                                for(auto j=0;j<(int)elem.size() && j < outDegree[(int)i];j++){
                                        f(i,elem[j],val);
                                }
                        }
                }
                if(outDegree[i] > IN_PLACE){
                        for(auto& elem : level2.at(i)){
                                f(i,elem,val);
                        }
                }
                if(outDegree[i] > PMA_SIZE){
                        for(auto elem : level3[i]){
                                f(i,elem,val);
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
                        for(auto elem: level1){
                                for(auto j=0;j<elem.size() && j < outDegree[(int)i];j++){
                                        count += f(i,elem[j],val);
                                }
                        }
                }
                if(outDegree[i] > IN_PLACE){
                        for(auto& elem : level2.at(i)){
                                count += f(i, elem, val);
                        }
                }
                if(outDegree[i] > PMA_SIZE){
                        for(auto elem : level3[i]){
                                count += f(i, elem, val);
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