#include <ne/Tm4cI2c.h>

template <typename T>
void ignore(const T&) {}   // suppresses "unused variable" warning

int main() {
    ne::Tm4cI2c *p = nullptr;
    ignore(p);
    
    return 0;
}
