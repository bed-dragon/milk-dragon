#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

int main() {
    std::vector<int> nums = {5, 2, 8, 1, 9, 3};

    // 排序
    std::sort(nums.begin(), nums.end());

    // 输出
    std::cout << "排序后: ";
    for (int n : nums) {
        std::cout << n << " ";
    }
    std::cout << std::endl;

    // 求和
    int sum = 0;
    for (int n : nums) {
        sum += n;
    }
    std::cout << "总和: " << sum << std::endl;

    return 0;
}
