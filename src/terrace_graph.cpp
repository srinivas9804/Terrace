#include "graph.h"
#include <iostream>

void TerraceGraph::add_edge(int u, int v, int w){
   if(degree[u] < IN_PLACE){
      level1[u][degree[u]] = v;
   }
   else if(degree[u] < IN_PLACE + PMA_SIZE){
      level2[u].insert(v);
   }
   else{
      level3[u].insert(v);
   }

   degree[u]++;
}
TerraceGraph::TerraceGraph(std::string filename, bool isSymmetric){
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
   degree.resize(nodes);
   level1.resize(nodes);
   level3.resize(nodes);
   std::vector<int> nodeIndex(nodes);
   for(int i=0;i<nodes;i++){
      inputFile >> nodeIndex[i];
   }
   for(int i=0;i<nodes - 1;i++){
      int count = nodeIndex[i+1] - nodeIndex[i];
      // std::cout << "nodeIndex[" << i << "]=" << nodeIndex[i] << std::endl;
      // std::cout << "nodeIndex[" << i+1 << "]=" << nodeIndex[i+1] << std::endl;
      std::cout << "neighbourCount[" << i << "]=" << count << std::endl; 
      for(int j=nodeIndex[i]; j<nodeIndex[i+1];j++){
         int v;
         inputFile >> v;
         add_edge(i,v,0);
         if(isSymmetric){
            add_edge(v,i,0);
         }
      }
   }
   std::cout << "neighbourCount[" << nodes-1 << "]=" << edges-nodeIndex[nodes-1] << std::endl; 
   for(int j=nodeIndex[nodes-1];j<edges;j++){
      int v;
      inputFile >> v;
      add_edge(nodes-1,v,0);
   }
   inputFile.close();
}

int main(){
   Graph graph("slashdot.adj");
   return 0;
}