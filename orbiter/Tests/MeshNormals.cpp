#include "Mesh.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main() {
    Mesh mesh;
    NTVERTEX vertices[5]{};
    vertices[1].x=1;vertices[2].y=1;vertices[3].x=2;
    const WORD indices[]={0,1,2,0,1,3}; // valid + collinear triangle; vertex 4 unused
    WORD copy[6];std::copy(indices,indices+6,copy);
    const auto group=mesh.AddGroup(vertices,5,copy,6,SPEC_DEFAULT,SPEC_DEFAULT,0,0,true);
    mesh.CalcNormals(group,false);
    auto result=mesh.GetGroup(group);
    for(unsigned i=0;i<5;++i) {
        const auto &v=result->Vtx[i];
        if(!std::isfinite(v.nx)||!std::isfinite(v.ny)||!std::isfinite(v.nz))throw std::runtime_error("nonfinite normal");
    }
    if(std::abs(result->Vtx[0].nz-1)>1e-6 || result->Vtx[4].nz!=0)throw std::runtime_error("incorrect normal");
    std::cout<<"Degenerate and unused vertices retain finite mesh normals\n";
}
