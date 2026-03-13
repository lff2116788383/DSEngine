#ifndef DSE_RENDER_QUEUE_H
#define DSE_RENDER_QUEUE_H

#include <vector>
#include <algorithm>
#include "render_pass.h"

class RenderQueue {
public:
    void AddPass(std::shared_ptr<RenderPass> pass) {
        passes_.push_back(pass);
    }

    void Execute() {
        // Sort passes if needed
        for (auto& pass : passes_) {
            pass->Execute();
        }
        passes_.clear();
    }

private:
    std::vector<std::shared_ptr<RenderPass>> passes_;
};

#endif // DSE_RENDER_QUEUE_H
