#ifndef DSE_RESOURCE_H
#define DSE_RESOURCE_H

#include <string>
#include <atomic>
#include <rttr/registration>

class Resource {
public:
    Resource() : reference_count_(0) {}
    virtual ~Resource() {}

    const std::string& GetPath() const { return path_; }
    void SetPath(const std::string& path) { path_ = path; }

    void AddReference() { reference_count_++; }
    void RemoveReference() { reference_count_--; }
    int GetReferenceCount() const { return reference_count_; }

    virtual bool Load(const std::string& path) = 0;

protected:
    std::string path_;
    std::atomic<int> reference_count_;

RTTR_ENABLE()
};

#endif // DSE_RESOURCE_H
