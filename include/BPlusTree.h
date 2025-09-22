#ifndef BPLUSTREE_H
#define BPLUSTREE_H

#include <cassert>
#include <functional>
#include <stack>
#include <queue>
#include <iostream>
#include <utility>
#include <shared_mutex>
#include <mutex>
#include <map>
#include <string>
#include <thread>
#include <fstream>
#include <vector>

template<int order, typename Key, typename Value, typename Compare = std::less<Key>>

class BPlusTree
{
private:
    struct Node
    {
        int n; // 节点的关键字个数
        bool IS_LEAF; // 是否是叶子节点
        Key keys[order]; // 节点键值的数组，具有唯一性和可排序性
        Value* values; // 叶子节点保存的值的数组
        std::shared_mutex mtx; // 读写锁

        /** 对于叶子节点而言，其为双向链表中结点指向前后节点的指针，ptr[0]表示前一个节点，ptr[1]表示后一个节点; 
        * 对于非叶子节点而言，其指向孩子节点，keys[i]的左子树是ptr[i]，右子树是ptr[i+1] 
        **/
        Node** ptr;

        Node(bool isLeaf)
        {
            this->n = 0;
            this->IS_LEAF = isLeaf;
 
            // 叶子节点
            if(this->IS_LEAF)
            {
                this->values = new Value[order];
                this->ptr = new Node*[2]; // 只需两个指针维护前后叶子（形成双链表）
				for(int i = 0; i < 2; i++)
                {
					this->ptr[i] = nullptr;
				}
            }
            else
            {
                // 非叶子节点
                this->values = nullptr;
                this->ptr = new Node*[order + 1]; // order + 1个子指针
				for(int i = 0; i < order + 1; i++)
                {
					this->ptr[i] = nullptr;
				}
            }
        }

        // 析构函数释放资源，只释放values和ptr，不递归删除子树（由外部统一管理）
        ~Node()
        {
            if(this->isLeaf())
            {
                delete[] this->values;
            }
            delete[] this->ptr;
        }

        // 二分查找，返回第一个大于等于该节点key的下标，
        inline int search(Key key,const Compare& compare) const noexcept
        {
            // 避免因为极端情况导致的查询效果低下
            if(!this->n || !compare(this->keys[0],key))
            {
                return 0;
            }

            if(this->n && compare(this->keys[this->n-1],key))
            {
                return this->n;
            } 

            int i = 1, j = this->n - 1;
            while(i <= j)
            {
                int mid = i + ((j - i) >> 1);
                if(this->keys[mid] == key)
                {
                    return mid;
                }

                if(compare(this->keys[mid],key)) 
                {
                    i = mid + 1;
                }
                else
                {
                    j = mid - 1;
                }
            }

            return i;
        }

        // 判断节点是否含有key
        inline bool hasKey(Key key,const Compare& compare) const noexcept
        {
            int arg = this->search(key, compare);
            return this->keys[arg] == key;
        }

        // inline Value search_recursive(Node* node, Key key) const 
        // {
        //     if (!node) return -1;

        //     if (node->isLeaf())
        //     {
        //         // 在叶子节点中线性查找（也可以用二分）
        //         for (int i = 0; i < node->n; ++i)
        //         {
        //             if (node->keys[i] == key)
        //             {
        //                 return node->values[i];
        //             }
        //         }
        //         return -1;
        //     }
    

        // // 非叶子节点：找到子树范围
        // int i = 0;
        // while (i < node->n && key > node->keys[i]) {
        //     ++i;
        // }
        // // 递归进入第 i 棵子树
        // return search_recursive(node->ptr[i], key);
        // }


        // 是否上溢出：节点 key 树 >= order -> 需要分裂
        inline bool isUpOver() const noexcept
        {
            return this->n >= order;
        }

        // 是否下溢出：节点 key 数 < (order -1) / 2 -> 需要合并或借位
        inline bool isDownOver() const noexcept
        {
            return this->n < ((order-1)>>1);
        }

        // 判断该节点是否为叶子节点
        inline bool isLeaf() const noexcept
        {
            return this->IS_LEAF;
        }
        
        // 在叶子节点插入一个键值对
        inline void insert(Key key,Value value,const Compare& compare)
        {
            assert(this->isLeaf());
            int arg = this->search(key,compare);

            // 后移数据腾出空间
            for(int i = this->n; i > arg; i--)
            {
                this->keys[i] = this->keys[i - 1];
                this->values[i] = this->values[i - 1];
            }
            this->keys[arg] = key;
            this->values[arg] = value;
            this->n++; 
        }

        // 在非叶子节点插入key和右子树
        inline void insert(Key key, Node* rightChild,const Compare& compare)
        {
            assert(!this->isLeaf());
            int arg = this->search(key,compare);
            for(int i = this->n; i > arg; i--)
            {
                this->keys[i] = this->keys[i-1];
				this->ptr[i+1] = this->ptr[i];
            }
            this->keys[arg] = key;
            this->ptr[arg+1] = rightChild;
            this->n++; 
        }

        // 更新节点
        inline void update(Key key,Value value,const Compare& compare)
        {
            assert(this->isLeaf());
            int arg = this->search(key,compare);
            this->values[arg] = value;
        }

        // 删除节点及其右子树
        inline void remove(Key key,const Compare& compare)
        {
            int arg = this->search(key,compare);
            for(int i=arg;i<this->n-1;i++){
                this->keys[i] = this->keys[i+1];
                if(!this->isLeaf()) this->ptr[i+1] = this->ptr[i+2];
                else this->values[i] = this->values[i+1];
            }
            if(!this->isLeaf()) this->ptr[this->n] = nullptr;
            this->n--;
        }

        // 上溢出(n >= order)的时候调用，分裂成左右子树，自身变成左子树，返回右子树
        inline Node* split()
        {
            Node* newNode = new Node(this->isLeaf());
            int mid = (order>>1);
            if(this->isLeaf())
            {
                for(int i=0,j=mid;j<this->n;i++,j++)
                {
                    newNode->keys[i] = this->keys[j];
                    newNode->values[i] = this->values[j];
                    newNode->n++;
                }
                this->insertNextNode(newNode);
            }
            else
            {
                newNode->ptr[0] = this->ptr[mid+1];
                this->ptr[mid+1] = nullptr;
                for(int i=0,j=mid+1; j<this->n;i++,j++)
                {
                    newNode->keys[i] = this->keys[j];
                    newNode->ptr[i+1] = this->ptr[j+1];
                    newNode->n++;
                    this->ptr[j+1] = nullptr;
                }
            }
			this->n = mid;
            return newNode;
        }

        // 非叶子节点下溢出(n < (order>>1))且兄弟无法借出节点时调用，和右兄弟合并
        inline void merge(Key key,Node *rightSibling) 
        {
            assert(!this->isLeaf());
            this->keys[this->n] = key;
            this->ptr[this->n+1] = rightSibling->ptr[0];
            this->n++;
            for(int i=0;i<rightSibling->n;i++){
                this->keys[this->n] = rightSibling->keys[i];
                this->ptr[this->n+1] = rightSibling->ptr[i+1];
                this->n++;
            }
            delete rightSibling;
        }

        // 叶子节点下溢出(n < (order>>1))且兄弟无法借出节点时调用，和右兄弟合并
        inline void merge(Node *rightSibling)
        {
            assert(this->isLeaf());
            for(int i=0;i<rightSibling->n;i++){
                this->keys[this->n] = rightSibling->keys[i];
                this->values[this->n] = rightSibling->values[i];
                this->n++;
            }
            this->removeNextNode();
            delete rightSibling;
        }

        // 双向链表中插入下一个叶子节点
        inline void insertNextNode(Node *nextNode)
        {
            assert(this->isLeaf() && nextNode);
            nextNode->ptr[1] = this->ptr[1];
            if(this->ptr[1]) this->ptr[1]->ptr[0] = nextNode;
            this->ptr[1] = nextNode;
            nextNode->ptr[0] = this;
        } 

        // 双向链表中删除下一个叶子节点
        inline void removeNextNode()
        {
            assert(this->isLeaf());
            if(this->ptr[1]->ptr[1]) this->ptr[1]->ptr[1]->ptr[0] = this;
            if(this->ptr[1]) this->ptr[1] = this->ptr[1]->ptr[1];
        }
    };

public:
    Compare compare;
    int size;
    Node *root; // B+树的根结点
    Node *head; // 叶子节点的头结点

    std::stack<Node*> findNodeByKey(Key key);
    void adjustNodeForUpOver(Node *node,Node* parent);
    void adjustNodeForDownOver(Node *node,Node* parent);
    void maintainAfterInsert(std::stack<Node*>& nodePathStack);
    void maintainAfterRemove(std::stack<Node*>& nodePathStack);

public:
    BPlusTree()
    {
        assert(order>=3);
        this->size = 0;
        this->root = nullptr;
        this->head = nullptr;
        this->compare = Compare();
        
    }
    int insert(Key key,Value value);
    int remove(Key key);

    // int find(Key key);
    void leafTraversal();
    void levelOrderTraversal();

    // 序列化接口
    void serialize(std::ostream& out);
    static BPlusTree* deserialize(std::istream& in);
};

/**
 * @brief  查找含有key的节点（需保证根节点存在）
 * @param  key 键值
 * @return 保存了查找路径的栈，栈顶即为含有key的节点
 */
template<int order,typename Key,typename Value,typename Compare>
std::stack<typename BPlusTree<order,Key,Value,Compare>::Node*> BPlusTree<order,Key,Value,Compare>::findNodeByKey(Key key)
{
    std::stack<Node*> nodePathStack;
    Node* node = this->root;
    // std::shared_mutex mtx;
	nodePathStack.push(node);
    while(!node->isLeaf())
    {
        int arg = node->search(key,this->compare);
        if(arg<node->n && node->keys[arg]==key) arg++;
        node = node->ptr[arg];
		nodePathStack.push(node);
    }
    return nodePathStack;
}

/**
 * @brief  插入键值对
 * @param  key 新的键
 * @param  value 新的值
 * @return int  0表示插入成功，1表示节点已存在，更新value
 */
template<int order, typename Key, typename Value, typename Compare>
int BPlusTree<order,Key,Value,Compare>::insert(Key key, Value value)
{
    if(this->root == nullptr)
    {
	   this->root = new Node(true);

       // 加上独占锁
       std::unique_lock<std::shared_mutex> lock(this->root->mtx);

       this->root->insert(key,value,this->compare);
       this->head = this->root;
       return 0;
    }

    std::stack<Node*> nodePathStack = this->findNodeByKey(key);
    Node *node = nodePathStack.top();
    
    // 加上独占锁
    std::unique_lock<std::shared_mutex> lock(node->mtx);

    if(node->hasKey(key, this->compare))
    {
        node->update(key, value, this->compare);
        return 1;
    }
    node->insert(key, value,this->compare);
    this->maintainAfterInsert(nodePathStack);
    this->size++;
    return 0;
}

/**
 * @brief  插入新数据后的维护
 * @param  nodePathStack 保存了因为插入而受到影响的节点的栈
 * @return void
 */
template<int order,typename Key,typename Value,typename Compare>
void BPlusTree<order,Key,Value,Compare>::maintainAfterInsert(std::stack<Node*>& nodePathStack)
{
    Node *node,*parent;
    node = nodePathStack.top();nodePathStack.pop();
    while(!nodePathStack.empty()){
        parent = nodePathStack.top();nodePathStack.pop();
        if(!node->isUpOver()) return ;
        this->adjustNodeForUpOver(node,parent);
        node = parent;
    }
    if(!node->isUpOver()) return ;
    this->root = new Node(false);
    parent = this->root;
    parent->ptr[0] = node;
    this->adjustNodeForUpOver(node, parent);
}

/**
 * @brief  调整上溢出节点
 * @param  node 上溢出节点
 * @param  parent 上溢出节点的父亲
 * @return void
 */
template<int order,typename Key,typename Value,typename Compare>
void BPlusTree<order,Key,Value,Compare>::adjustNodeForUpOver(Node *node,Node* parent){
    // For node As LeafNode
    // parent:        ...  ...                   ... mid ...
    //                   /           =====>         /   | 
    // node:      [left] mid [right]           [left] mid:[right]
    // For node As Node
    // parent:        ...  ...                   ... mid ...
    //                   /           =====>         /   | 
    // node:      [left] mid [right]           [left] [right]
    //
    int mid = (order>>1);
    Key key = node->keys[mid];
    Node *rightChild = node->split();
    parent->insert(key,rightChild,this->compare);
}

/**
 * @brief  根据键删除数据
 * @param  key 要被删除的数据的键
 * @return int  0表示删除成功，1表示键不存在，删除失败
 */
template<int order,typename Key,typename Value,typename Compare>
int BPlusTree<order,Key,Value,Compare>::remove(Key key)
{
    if(this->root == nullptr)
    {
        return 1;
    }

    std::stack<Node*> nodePathStack = this->findNodeByKey(key);
    Node *node =nodePathStack.top();

    // 加上独占锁
    std::unique_lock<std::shared_mutex> lock(node->mtx);
    if(!node->hasKey(key,this->compare))
    {
        return 1;
    }

    node->remove(key,this->compare);
    this->maintainAfterRemove(nodePathStack);
    this->size--;

    return 0;
}










/**
 * @brief  根据键删除数据
 * @param  key 要被删除的数据的键
 * @return int  0表示删除成功，1表示键不存在，删除失败
 */
// template<int order,typename Key,typename Value,typename Compare>
// int BPlusTree<order,Key,Value,Compare>::find(Key key)
// {
//     return search_recursive(root, key);
// }







/**
 * @brief  删除数据后的维护
 * @param  nodePathStack 保存了因为删除而受到影响的节点的栈
 * @return void
 */
template<int order,typename Key,typename Value,typename Compare>
void BPlusTree<order,Key,Value,Compare>::maintainAfterRemove(std::stack<Node*>& nodePathStack){
    Node *node,*parent;
    node = nodePathStack.top();nodePathStack.pop();
    while(!nodePathStack.empty())
    {
        parent = nodePathStack.top();nodePathStack.pop();
        if(!node->isDownOver())
        {
            return;
        }

        this->adjustNodeForDownOver(node,parent);
        node = parent;   
    }
    if(this->root->n)
    {
        return;
    }

    if(!this->root->isLeaf())
    {
        this->root = node->ptr[0];
    }
	else
    {
		this->root = nullptr;
		this->head = nullptr;
    }
    delete node;
}

/**
 * @brief  调整下溢出节点
 * @param  node 下溢出的节点
 * @param  parent 下溢出节点的父亲
 * @return void
 */
template<int order,typename Key,typename Value,typename Compare>
void BPlusTree<order,Key,Value,Compare>::adjustNodeForDownOver(Node *node,Node* parent)
{
    int mid = ((order-1)>>1);
	int arg = -1;
	if(node->n) 
    {
        arg = parent->search(node->keys[0],this->compare);
    }
	else while(parent->ptr[++arg]!=node);	
    Node* left = arg > 0 ? parent->ptr[arg-1] : nullptr;
    Node* right = arg < parent->n ? parent->ptr[arg+1] : nullptr;
    // case 1: 左兄弟或右兄弟可以借出一个数据
    if((left && left->n > mid) || (right && right->n > mid))
    {
        // 左兄弟借出一个数据
        // For node As LeafNode
        //  parent:       ... key ...                        ... last ... 
        //                  /     \         =====>             /      \ 
        //         [......]:last  [node]                  [......]   last:[node]
        // For node As Node
        //  parent:       ... key ...                        ... last ... 
        //                  /     \         =====>             /      \ 
        //         [......]:last  [node]                  [......]   key:[node]
        if(left && left->n > mid)
        {
            if(node->isLeaf())
            {
                node->insert(left->keys[left->n-1],left->values[left->n-1],this->compare);
            }
            else
            {
                node->insert(parent->keys[arg-1],node->ptr[0],this->compare);
                node->ptr[0] = left->ptr[left->n];
            }
            parent->keys[arg-1] = left->keys[left->n-1];
            left->remove(left->keys[left->n-1], this->compare);
        }

        // 右兄弟借出一个数据
        // For node As LeafNode
		//  parent:     ... key ...                        ... second ...
		//                /     \            =====>            /       \ 
		//          [node] first:second:[......]       [node]:first  second:[......]
        // For node As Node
        //  parent:     ... key ...                        ... first ...
		//                /     \            =====>            /       \ 
		//           [node] first:[......]               [node]:key  [......]
        else if(right && right->n > mid)
        {
            if(node->isLeaf())
            {
                node->insert(right->keys[0],right->values[0],this->compare);
				right->remove(right->keys[0],this->compare);
				parent->keys[arg] = right->keys[0];
            }
            else
            {
                node->insert(parent->keys[arg],right->ptr[0],this->compare);
                right->ptr[0] = right->ptr[1];
				parent->keys[arg] = right->keys[0];
				right->remove(right->keys[0],this->compare);
            }            
        }
        return;
    }

    // case 2: 左兄弟或右兄弟都不能借出一个数据
    if(left)
    {
        // 和左兄弟合并
        // For node As LeafNode
        //  parent:       ... key ...                        ...  ...
        //                  /     \         =====>              /     
        //              [left]  [node]                  [left]:[node]   
        // For node As Node
        //  parent:       ... key ...                        ...  ...
        //                  /     \         =====>              /     
        //              [left]  [node]                  [left]:key:[node]
        Key key = parent->keys[arg-1];
        if(left->isLeaf())
        {
            left->merge(node);
        }
        else 
        {
            left->merge(key,node);
        }

        parent->remove(key,this->compare);
    }
    else if(right)
    {
        // 和右兄弟合并
        // For node As LeafNode
        //  parent:       ... key ...                        ...  ...
        //                  /     \         =====>              /     
        //              [node]  [right]                  [node]:[right]  
        // For node As Node
        //  parent:       ... key ...                        ...  ...
        //                  /     \         =====>              /     
        //              [node]  [right]                  [node]:key:[right]    
        Key key = parent->keys[arg];
        if(node->isLeaf())
        {
            node->merge(right);
        }
        else
        {
            node->merge(key,right);
        }

        parent->remove(key,this->compare);
    }
}

/**
 * @brief  B+树的叶子节点层的遍历
 * @return void
 */
template<int order, typename Key, typename Value, typename Compare>
void BPlusTree<order, Key, Value, Compare>::leafTraversal()
{
    Node* p = this->head;
    while(p)
    {
        // 加上共享锁
        std::shared_lock<std::shared_mutex> lock(p->mtx);
        for(int i = 0;i < p->n; i++)
        {
            std::cout << p->keys[i] << ' ';
        }
        std::cout << "| ";

        p = p->ptr[1];
    }

    

    std::cout << std::endl;
}

/**
 * @brief  B+树的层序遍历
 * @return void
 */
template<int order,typename Key,typename Value,typename Compare>
void BPlusTree<order, Key, Value, Compare>::levelOrderTraversal()
{
    std::queue<Node*> q;
    q.push(this->root);

    while(!q.empty())
    {
        int layerSize = q.size(); // 该层的节点数量

        // 输出该层的节点的关键字值
        while(layerSize--)
        {
            Node *node = q.front();

            // 加上共享锁
            std::shared_lock<std::shared_mutex> lock(node->mtx);

            q.pop();
            if(!node)
            {
                continue;
            }

            int i = 0;

            // 打印该节点的关键字
            for(i = 0; i< node->n; i++)
            {
                std::cout << node->keys[i] << " ";
                if(!node->isLeaf()) 
                {
                    q.push(node->ptr[i]);
                }
            }
			std::cout << "| ";

            if(!node->isLeaf())
            {
                q.push(node->ptr[i]);
            }
        }

        // 该层输出结束
        std::cout << std::endl;
    }
}

/**
 * @brief 序列化整个B+树到输出流
 * @param out 输出流（可以是文件、内存等）
 */
template<int order, typename Key, typename Value, typename Compare>
void BPlusTree<order, Key, Value, Compare>::serialize(std::ostream& out)
{
    // 这里简化处理：只保存数据，不保存 head 指针关系（重建时重新链接叶子）
    int tree_order = order;

    // 先写入树的order和size, 构成头部信息
    out.write(reinterpret_cast<const char*>(&tree_order), sizeof(tree_order));
    out.write(reinterpret_cast<const char*>(&size), sizeof(size));

    // 按先序遍历存储节点结构
    std::function<void(Node*)> serialize_node = [&](Node* node)
    {
        // 序列化空节点
        if (!node)
        {
            bool is_null = true;
            out.write(reinterpret_cast<const char*>(&is_null), sizeof(is_null));
            return;
        }

        bool is_null = false;
        out.write(reinterpret_cast<const char*>(&is_null), sizeof(is_null));  // Not null
        out.write(reinterpret_cast<const char*>(&node->n), sizeof(node->n));
        out.write(reinterpret_cast<const char*>(&node->IS_LEAF), sizeof(node->IS_LEAF));

        // Write keys
        out.write(reinterpret_cast<const char*>(node->keys), node->n * sizeof(Key));

        if (node->isLeaf())
        {
            // Write values
            out.write(reinterpret_cast<const char*>(node->values), node->n * sizeof(Value));
        }
        else
        {
            // Recursively write children
            for (int i = 0; i <= node->n; i++)
            {
                serialize_node(node->ptr[i]);
            }
        }
    };

    serialize_node(root);
}

/**
 * @brief 从输入流反序列化构建新的 B+ 树
 * @param in 输入流
 * @return BPlusTree* 新的 B+ 树实例
 */
template<int order, typename Key, typename Value, typename Compare>
BPlusTree<order, Key, Value, Compare>* BPlusTree<order, Key, Value, Compare>::deserialize(std::istream& in)
{
    int saved_order;
    in.read(reinterpret_cast<char*>(&saved_order), sizeof(saved_order));
    if (saved_order != order)
    {
        std::cerr << "Error: Serialized tree order (" << saved_order
                  << ") does not match current template order (" << order << ")" << std::endl;
        return nullptr;
    }

    auto tree = new BPlusTree();
    in.read(reinterpret_cast<char*>(&tree->size), sizeof(tree->size));

    std::function<Node*(void)> deserialize_node = [&]() -> Node* 
    {
        bool is_null;
        in.read(reinterpret_cast<char*>(&is_null), sizeof(is_null));
        if (is_null) return nullptr;

        int n;
        bool is_leaf;
        in.read(reinterpret_cast<char*>(&n), sizeof(n));
        in.read(reinterpret_cast<char*>(&is_leaf), sizeof(is_leaf));

        Node* node = new Node(is_leaf);
        node->n = n;

        // Read keys
        in.read(reinterpret_cast<char*>(node->keys), n * sizeof(Key));

        if (is_leaf)
        {
            // Read values
            in.read(reinterpret_cast<char*>(node->values), n * sizeof(Value));
        } 
        else
        {
            // Read children recursively
            for (int i = 0; i <= n; i++)
            {
                node->ptr[i] = deserialize_node();
            }
        }

        return node;
    };

    tree->root = deserialize_node();

    // 反序列化后重建叶节点链接
    if (tree->root) 
    {
        std::vector<Node*> leaves;
        std::function<void(Node*)> collect_leaves = [&](Node* node)
        {
            if (!node) 
            {
                return;
            }

            if (node->isLeaf()) 
            {
                leaves.push_back(node);
            } 
            else
            {
                for (int i = 0; i <= node->n; i++)
                {
                    collect_leaves(node->ptr[i]);
                }
            }
        };
        collect_leaves(tree->root);

        // Link leaves
        tree->head = leaves.empty() ? nullptr : leaves[0];
        for (size_t i = 0; i < leaves.size(); ++i) 
        {
            leaves[i]->ptr[1] = (i + 1 < leaves.size()) ? leaves[i + 1] : nullptr;
            if (i > 0) 
            {
                leaves[i]->ptr[0] = leaves[i - 1];
            }
            else
            {
                leaves[i]->ptr[0] = nullptr;
            }
        }
    }

    return tree;
}

#endif 