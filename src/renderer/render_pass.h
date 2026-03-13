#ifndef DSE_RENDER_PASS_H
#define DSE_RENDER_PASS_H

#include <vector>
#include <memory>
#include "render_command.h" // You might need to adjust includes based on actual structure

class RenderPass {
public:
    enum class Type {
        Geometry,
        Lighting,
        PostProcess,
        UI
    };

    RenderPass(Type type) : type_(type) {}
    virtual ~RenderPass() {}

    virtual void Execute() = 0;

protected:
    Type type_;
    std::vector<std::shared_ptr<RenderCommand>> commands_;
};

#endif // DSE_RENDER_PASS_H
