#ifndef DSE_COMMAND_IMPL_H
#define DSE_COMMAND_IMPL_H

#include "command.h"
#include "component/game_object.h"
#include "component/transform.h"
#include <glm/glm.hpp>

// Command to move a GameObject
class CmdMove : public ICommand {
public:
    CmdMove(GameObject* target, const glm::vec3& new_pos) 
        : target_(target), new_pos_(new_pos) {
        if (target_) {
            Transform* t = target_->GetComponent<Transform>();
            if (t) old_pos_ = t->position();
        }
    }

    virtual void Execute() override {
        if (!target_) return;
        Transform* t = target_->GetComponent<Transform>();
        if (t) t->set_position(new_pos_);
    }

    virtual void Undo() override {
        if (!target_) return;
        Transform* t = target_->GetComponent<Transform>();
        if (t) t->set_position(old_pos_);
    }

    virtual std::string GetDescription() const override { return "Move GameObject"; }

private:
    GameObject* target_;
    glm::vec3 old_pos_;
    glm::vec3 new_pos_;
};

// Command to create a GameObject
class CmdCreateGameObject : public ICommand {
public:
    CmdCreateGameObject(const std::string& name, GameObject* parent = nullptr) 
        : name_(name), parent_(parent), created_object_(nullptr) {}
    
    virtual ~CmdCreateGameObject() {
        // If undone and not redone, we own the memory of the created object?
        // In a real system, we need careful memory management here.
    }

    virtual void Execute() override {
        created_object_ = new GameObject(name_.c_str());
        if (parent_) created_object_->SetParent(parent_);
    }

    virtual void Undo() override {
        if (created_object_) {
            // Remove from parent and delete (or hide/deactivate)
            // For simplicity, let's just delete it, but in real undo/redo we might cache it.
            delete created_object_;
            created_object_ = nullptr;
        }
    }

private:
    std::string name_;
    GameObject* parent_;
    GameObject* created_object_;
};

#endif // DSE_COMMAND_IMPL_H
