#include "Helper.hpp"

namespace YourStrategy
{
class MyStrategy : public ct::helper::Strategy
{
   public:
    void execute() override { /* Trading logic */ }
};
} // namespace YourStrategy

extern "C" ct::helper::Strategy *createStrategy()
{
    return new YourStrategy::MyStrategy();
}
