#include "Helper.hpp"

namespace YourStrategy
{
class MyStrategy : public Helper::Strategy
{
   public:
    void execute() override { /* Trading logic */ }
};
} // namespace YourStrategy

extern "C" Helper::Strategy *createStrategy()
{
    return new YourStrategy::MyStrategy();
}
