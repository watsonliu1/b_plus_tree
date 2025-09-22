#include "../include/BPlusTree.h"
#include <chrono>
#include <random>
#include <vector>
#include <iostream>


// B+树插入性能测试
void pref_test()
{
   constexpr int ORDER = 10;
   constexpr long N = 10000000;  // 一千万条数据  

    BPlusTree<ORDER, int, int> tree;

    std::vector<int> keys(N);
    std::vector<int> values(N);

    // 生成 0 ~ N-1 的序列并打乱，避免有序插入导致退化
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), std::mt19937{});

    // 所有 value 等于 key（便于验证）
    for (long i = 0; i < N; ++i) 
    {
        values[i] = keys[i];
    }

    std::cout << "Starting insertion of " << N << " key-value pairs...\n";

    auto start_time = std::chrono::steady_clock::now();

    // 插入数据
    size_t inserted = 0;
    for (long i = 0; i < N; ++i)
    {
        tree.insert(keys[i], values[i]);
        ++inserted;

        if (inserted % (N / 10) == 0)
        {
            std::cout << "[Insert] Progress: " << inserted << " / " << N << "\n";
        }
    }

    auto insert_end = std::chrono::steady_clock::now();
    std::cout << "Insertion completed in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(insert_end - start_time).count()
              << " ms\n";
}

// 功能测试
void func_test()
{
    constexpr int ORDER = 3; // 使用3阶B+树测试
    using TreeType = BPlusTree<ORDER, int, std::string>;

    TreeType* tree = new TreeType();

    std::cout << "=== 功能测试开始 ===" << std::endl;

    // 插入测试
    assert(tree->insert(10, "value_10") == 0);
    assert(tree->insert(20, "value_20") == 0);
    assert(tree->insert(5,  "value_5")  == 0);
    assert(tree->insert(15, "value_15") == 0);
    assert(tree->insert(25, "value_25") == 0);
    assert(tree->insert(18, "value_18") == 0);
    assert(tree->insert(30, "value_30") == 0);

    std::cout << "插入7个键后，叶子层遍历：" << std::endl;
    tree->leafTraversal(); 

    std::cout << "层序遍历：" << std::endl;
    tree->levelOrderTraversal();


    // 删除测试
    assert(tree->remove(5) == 0);  // 成功删除
    assert(tree->remove(5) == 1);  // 不存在，失败
    assert(tree->remove(18) == 0);
    assert(tree->remove(100) == 1); // 不存在

    std::cout << "删除键5和18后，叶子层遍历：" << std::endl;
    tree->leafTraversal(); // 观察是否仍有序
}

void serialize_test()
{
    // 创建并插入数据
    int arr[] ={70, 20, 150, 90, 40, 130, 10, 180, 60, 110, 30, 200, 80, 170, 50, 140, 100, 160, 120, 190};
    BPlusTree<3, int, int> btree;

    std::cout << "=== 序列化、反序列化测试开始 ===" << std::endl;

    for(int key: arr)
    {
        btree.insert(key, 0);
    }
    btree.leafTraversal();

    // 序列化到文件
    std::ofstream ofs("bPlusTree.dat", std::ios::binary);
    btree.serialize(ofs);
    ofs.close();

    // 反序列化
    std::ifstream ifs("bPlusTree.dat", std::ios::binary);
    auto restored_tree = BPlusTree<3, int, int>::deserialize(ifs);
    ifs.close();

    if (restored_tree)
    {
        restored_tree->leafTraversal();
        delete restored_tree;
    }
}

int main()
{
    
    pref_test();// 性能测试
    serialize_test(); // 序列化测试
    func_test();// 功能测试
   
    return 0;
}