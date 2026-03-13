//
// Created by captainchen on 2021/5/11.
//

#ifndef UNTITLED_MESH_RENDERER_H
#define UNTITLED_MESH_RENDERER_H

#include <memory>
#include <glm/glm.hpp>
#include "component/component.h"

class Material;
class MeshFilter;
class Texture2D;
class MeshRenderer:public Component{
public:
    MeshRenderer();
    virtual ~MeshRenderer();

    void SetMaterial(Material* material);//设置Material
    Material* material(){return material_;}

    void set_sorting_layer(int layer){sorting_layer_=layer;}
    int sorting_layer() const {return sorting_layer_;}

    void set_order_in_layer(int order){order_in_layer_=order;}
    int order_in_layer() const {return order_in_layer_;}

    virtual void Render();//渲染
private:
    Material* material_;

    int sorting_layer_ = 0;
    int order_in_layer_ = 0;

    unsigned int vertex_buffer_object_handle_=0;//顶点缓冲区对象句柄
    unsigned int vertex_array_object_handle_=0;//顶点数组对象句柄

RTTR_ENABLE();
};


#endif //UNTITLED_MESH_RENDERER_H
