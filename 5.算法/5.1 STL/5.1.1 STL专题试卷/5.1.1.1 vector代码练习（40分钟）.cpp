#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <vector>

// C++17。只修改 TODO；启用断言，不要定义 NDEBUG。
std::size_t eraseInvalid(std::vector<int>& scores) {
    // TODO：用迭代器循环和单元素 erase 删除无效成绩，返回删除数量。
    return 0;
}

bool insertBeforeFirst(std::vector<int>& scores, int target, int value) {
    // TODO：只在第一个 target 前插入一次，返回是否找到目标。
    return false;
}

void sortUniqueDesc(std::vector<int>& scores) {
    // TODO：使用 sort、unique、区间 erase 完成降序去重。
}

void print(const char* label, const std::vector<int>& scores) {
    std::cout << label << " [size=" << scores.size()
              << ", capacity=" << scores.capacity() << "]: ";
    for (int score : scores) {
        std::cout << score << ' ';
    }
    std::cout << '\n';
}

void testEdges() {
    std::vector<int> empty;
    auto removed = eraseInvalid(empty);
    assert(removed == 0 && empty.empty());
    bool inserted = insertBeforeFirst(empty, 60, 75);
    assert(!inserted && empty.empty());
    sortUniqueDesc(empty);
    assert(empty.empty());

    std::vector<int> invalid{-1, 101, -8, 150};
    removed = eraseInvalid(invalid);
    assert(removed == 4 && invalid.empty());

    std::vector<int> consecutive{-1, -2, 60, 101, 102, 100};
    removed = eraseInvalid(consecutive);
    assert(removed == 4);
    assert((consecutive == std::vector<int>{60, 100}));

    std::vector<int> boundaries{0, 100};
    removed = eraseInvalid(boundaries);
    assert(removed == 0);
    assert((boundaries == std::vector<int>{0, 100}));

    std::vector<int> repeated{88, 88, 88};
    sortUniqueDesc(repeated);
    assert((repeated == std::vector<int>{88}));

    std::vector<int> positions{60, 80};
    inserted = insertBeforeFirst(positions, 60, 50);
    assert(inserted);
    assert((positions == std::vector<int>{50, 60, 80}));
    inserted = insertBeforeFirst(positions, 80, 70);
    assert(inserted);
    assert((positions == std::vector<int>{50, 60, 70, 80}));
    inserted = insertBeforeFirst(positions, 99, 75);
    assert(!inserted);
    assert((positions == std::vector<int>{50, 60, 70, 80}));

    std::vector<int> same{60, 60};
    inserted = insertBeforeFirst(same, 60, 60);
    assert(inserted);
    assert((same == std::vector<int>{60, 60, 60}));

    std::vector<int> full{60};
    while (full.size() < full.capacity()) {
        full.push_back(80);
    }
    const auto oldSize = full.size();
    const auto oldCapacity = full.capacity();
    inserted = insertBeforeFirst(full, 60, 75);
    assert(inserted && full.size() == oldSize + 1);
    assert(full.capacity() > oldCapacity);
    assert(full[0] == 75 && full[1] == 60);
    for (std::size_t i = 2; i < full.size(); ++i) {
        assert(full[i] == 80);
    }
}

int main() {
    const std::vector<int> raw{88, -1, 60, 105, 88, 59, 100, 60, 0};
    std::vector<int> scores;

    // TODO：预留至少 20 个元素的容量，暂时不添加元素。

    assert(scores.empty() && scores.capacity() >= 20);
    const auto reservedCapacity = scores.capacity();
    print("reserved", scores);

    // TODO：遍历 raw，用 push_back 逐个录入。

    assert(scores == raw);
    print("loaded", scores);

    const auto removed = eraseInvalid(scores);
    assert(removed == 2);
    assert((scores == std::vector<int>{88, 60, 88, 59, 100, 60, 0}));
    assert(scores.capacity() == reservedCapacity);
    print("cleaned", scores);

    const bool inserted = insertBeforeFirst(scores, 60, 75);
    assert(inserted);
    assert((scores == std::vector<int>{88, 75, 60, 88, 59, 100, 60, 0}));
    assert(scores.capacity() == reservedCapacity);
    print("inserted", scores);

    sortUniqueDesc(scores);
    assert((scores == std::vector<int>{100, 88, 75, 60, 59, 0}));
    assert(scores.capacity() == reservedCapacity);
    print("sorted and unique", scores);

    // TODO：把 size 扩到 8，新增元素填 -1。

    assert((scores == std::vector<int>{100, 88, 75, 60, 59, 0, -1, -1}));
    assert(scores.capacity() == reservedCapacity);
    print("resize to 8", scores);

    // TODO：只保留前 3 个元素，使用 resize。

    assert((scores == std::vector<int>{100, 88, 75}));
    assert(scores.capacity() == reservedCapacity);
    print("resize to 3", scores);

    // TODO：清空元素，使用 clear。

    assert(scores.empty() && scores.capacity() == reservedCapacity);
    print("cleared", scores);

    testEdges();
    std::cout << "All tests passed.\n";
}

// TODO：每条用一两句话回答。
// 1. 为什么 reserve(20) 后不能直接写 scores[0] = 88？
// 2. erase 返回的迭代器指向哪里？删除后无条件 ++it 有什么问题？
// 3. eraseInvalid 的最坏时间复杂度是多少？reserve 能消除什么开销，不能消除什么开销？
