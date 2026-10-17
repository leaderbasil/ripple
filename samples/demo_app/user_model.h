#ifndef ALGO1_DEMO_USER_MODEL_H
#define ALGO1_DEMO_USER_MODEL_H

#include <string>

namespace algo1_demo {

/// In-memory stand-in for the project's data model.
class UserModel {
public:
    UserModel() : name_("anonymous"), visits_(0) {}
    explicit UserModel(std::string name) : name_(std::move(name)) {}

    const std::string& name() const { return name_; }

    void addVisit() { ++visits_; }
    int visits() const { return visits_; }

private:
    std::string name_;
    int visits_;
};

}  // namespace algo1_demo

#endif
