//
// Created by captain on 2021/4/6.
//
#define STB_TRUETYPE_IMPLEMENTATION
#include "texture_2d.h"
#include <fstream>
#include "timetool/stopwatch.h"
#include "stb/stb_truetype.h"
#include "stb/stb_image.h"
#include "utils/debug.h"
#include "app/application.h"
#include "render_device/render_task_producer.h"
#include "render_device/gpu_resource_mapper.h"


using std::ifstream;
using std::ios;
using timetool::StopWatch;

Texture2D::Texture2D() : mipmap_level_(0), width_(0), height_(0), gl_texture_format_(0), texture_handle_(0)
{

}

Texture2D::~Texture2D() {
    if(texture_handle_ > 0){
        RenderTaskProducer::ProduceRenderTaskDeleteTextures(1,&texture_handle_);
    }
}

void Texture2D::UpdateSubImage(int x, int y, int width, int height, unsigned int client_format, unsigned int data_type,
                               unsigned char *data,unsigned int data_size) {
    if(width<=0 || height<=0){
        return;
    }
    RenderTaskProducer::ProduceRenderTaskUpdateTextureSubImage2D(texture_handle_, x, y, width, height, client_format, data_type, data, data_size);
}

Texture2D* Texture2D::LoadFromFile(std::string image_file_path)
{
    if(image_file_path.empty()){
        DEBUG_LOG_ERROR("image_file_path empty");
        return nullptr;
    }
    Texture2D* texture2d=new Texture2D();

    // 尝试读取 cpt 压缩纹理文件
    std::string full_path = Application::data_path() + image_file_path;
    ifstream input_file_stream(full_path, ios::in | ios::binary);
    
    if(input_file_stream.is_open()){
        // 检查是否为 cpt 格式
        char header[3];
        input_file_stream.read(header, 3);
        input_file_stream.close();
        
        if(strncmp(header, "cpt", 3) == 0){
            // 是 cpt 格式
            input_file_stream.open(full_path, ios::in | ios::binary);
            CptFileHead cpt_file_head;
            input_file_stream.read((char*)&cpt_file_head, sizeof(CptFileHead));

            unsigned char* data =(unsigned char*)malloc(cpt_file_head.compress_size_);
            input_file_stream.read((char*)data, cpt_file_head.compress_size_);
            input_file_stream.close();

            texture2d->gl_texture_format_=cpt_file_head.gl_texture_format_;
            texture2d->width_=cpt_file_head.width_;
            texture2d->height_=cpt_file_head.height_;
            texture2d->texture_handle_=GPUResourceMapper::GenerateTextureHandle();

            // 发出任务：创建压缩纹理
            RenderTaskProducer::ProduceRenderTaskCreateCompressedTexImage2D(texture2d->texture_handle_, texture2d->width_,
                                                                            texture2d->height_, texture2d->gl_texture_format_,
                                                                            cpt_file_head.compress_size_, data);

            free(data);
            return texture2d;
        }
    }
    
    // 不是 cpt 格式，使用 stb_image 加载常见图像格式
    int width, height, channels;
    unsigned char* data = stbi_load(full_path.c_str(), &width, &height, &channels, 0);
    if(data == nullptr){
        DEBUG_LOG_ERROR("Failed to load image: {}", image_file_path);
        return texture2d;
    }
    
    texture2d->width_ = width;
    texture2d->height_ = height;
    texture2d->texture_handle_ = GPUResourceMapper::GenerateTextureHandle();
    
    // 根据通道数设置纹理格式
    GLenum internal_format, format;
    if(channels == 4){
        internal_format = GL_RGBA;
        format = GL_RGBA;
    } else if(channels == 3){
        internal_format = GL_RGB;
        format = GL_RGB;
    } else if(channels == 1){
        internal_format = GL_RED;
        format = GL_RED;
    } else {
        internal_format = GL_RGB;
        format = GL_RGB;
    }
    
    texture2d->gl_texture_format_ = internal_format;
    
    // 发出任务：创建纹理
    RenderTaskProducer::ProduceRenderTaskCreateTexImage2D(texture2d->texture_handle_, width, height, internal_format, format, GL_UNSIGNED_BYTE, width * height * channels, data);
    
    stbi_image_free(data);
    return texture2d;
}

Texture2D *Texture2D::Create(unsigned short width, unsigned short height, unsigned int server_format,unsigned int client_format,
                             unsigned int data_type,unsigned char* data,unsigned int data_size) {
    Texture2D* texture2d=new Texture2D();
    texture2d->gl_texture_format_=server_format;
    texture2d->width_=width;
    texture2d->height_=height;
    texture2d->texture_handle_=GPUResourceMapper::GenerateTextureHandle();

    // 发出任务：创建纹理
    RenderTaskProducer::ProduceRenderTaskCreateTexImage2D(texture2d->texture_handle_,texture2d->width_, texture2d->height_,texture2d->gl_texture_format_,client_format, data_type,data_size,data);

    return texture2d;
}


