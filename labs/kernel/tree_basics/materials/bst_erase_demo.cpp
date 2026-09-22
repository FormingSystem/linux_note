#include <iostream>
#include <new>

/*
 * BST 节点结构
 * key   : 当前节点保存的键值
 * left  : 指向左子树
 * right : 指向右子树
 */
struct bst_node {
	int key;
	bst_node *left;
	bst_node *right;
};

/*
 * 插入结果
 * ok        : 插入成功
 * duplicate : 已存在，保持原树不变
 * no_memory : 内存分配失败
 */
enum class bst_insert_result {
	ok,
	duplicate,
	no_memory,
};

/*
 * 创建一个新节点
 *
 * 参数：
 * key - 节点键值
 *
 * 返回值：
 * 成功时返回新节点地址
 * 失败时返回 nullptr
 *
 * 说明：
 * 这里使用 nothrow 版本的 new，
 * 分配失败时返回 nullptr；本例只有整数和指针的平凡初始化。
 * 若以后加入会抛异常的构造器，不能据此保证整个表达式不抛。
 */
bst_node *bst_create_node(int key) noexcept
{
	return new (std::nothrow) bst_node { key, nullptr, nullptr };
}

/*
 * 向 BST 中插入一个键值
 *
 * 参数：
 * root - 树根引用
 * key  - 待插入键值
 *
 * 返回值：
 * ok        : 插入成功
 * duplicate : 已存在，未分配节点
 * no_memory : 节点创建失败
 *
 * 说明：
 * 这里使用 bst_node **link 的方式向下查找插入位置。
 * link 一开始指向根指针本身，
 * 后续根据比较结果不断改为：
 *   - &(*link)->left
 *   - &(*link)->right
 *
 * 这样做的好处是：
 * 找到空位置后，可以直接通过 *link = node 完成挂接，
 * 不需要额外记录父节点再区分挂左还是挂右。
 *
 * 注意：
 * 这个实现不允许重复键值。
 * 重复键返回 duplicate，由调用者决定如何处理，树保持不变。
 */
bst_insert_result bst_insert(bst_node *&root, int key) noexcept
{
	bst_node **link = &root;
	bst_node *node;

	/* 沿着 BST 的比较规则向下查找插入位置 */
	while (*link) {
		if (key < (*link)->key)
			link = &(*link)->left;
		else if (key > (*link)->key)
			link = &(*link)->right;
		else {
			/* 唯一键策略：查重属于正常结果，不终止进程。 */
			return bst_insert_result::duplicate;
		}
	}

	/* 走到这里说明 *link 为空，当前位置就是插入点 */
	node = bst_create_node(key);
	if (!node)
		return bst_insert_result::no_memory;

	/* 直接把新节点挂到找到的空位置 */
	*link = node;
	return bst_insert_result::ok;
}

/*
 * 查找一棵子树中的最小节点
 *
 * 参数：
 * root - 子树根节点
 *
 * 返回值：
 * 最小节点地址；若 root 为空则返回 nullptr
 *
 * 说明：
 * BST 中最小节点一定在最左侧路径上。
 */
bst_node *bst_find_min(bst_node *root)
{
	if (!root)
		return nullptr;

	while (root->left)
		root = root->left;

	return root;
}

/*
 * 删除 BST 中指定键值的节点
 *
 * 参数：
 * root - 当前子树根节点
 * key  - 待删除键值
 *
 * 返回值：
 * 删除完成后，这棵子树的新根节点
 *
 * 这是一个非常关键的设计点：
 * 本函数不是“原地只改当前节点”，
 * 而是“删除后返回新的子树根”，
 * 这样上层调用者就可以写成：
 *
 *   root->left = bst_erase(root->left, key);
 *   root->right = bst_erase(root->right, key);
 *
 * 从而自然完成“子树回接”。
 */
bst_node *bst_erase(bst_node *root, int key)
{
	bst_node *succ;

	/* 空树，说明没找到待删节点 */
	if (!root)
		return nullptr;

	/*
	 * 若 key 更小，说明待删节点在左子树中。
	 * 删除完成后，左子树的根可能发生变化，
	 * 所以必须把返回值重新接回 root->left。
	 */
	if (key < root->key) {
		root->left = bst_erase(root->left, key);
		return root;
	}

	/*
	 * 若 key 更大，说明待删节点在右子树中。
	 * 同理，删除完成后要把新根重新接回 root->right。
	 */
	if (key > root->key) {
		root->right = bst_erase(root->right, key);
		return root;
	}

	/*
	 * 走到这里说明：
	 * 当前 root 就是待删除节点。因为 > 或者 < 都不适用于 ==，到这里自然是判断 == 了
	 *
	 * 接下来按孩子数量分类讨论。
	 */

	/*
	 * 情况 1：没有左孩子
	 *
	 * 包含两种子情况：
	 * 1) 左右孩子都没有       -> 叶子删除
	 * 2) 只有右孩子           -> 单孩子删除
	 *
	 * 统一处理方式：
	 * 当前节点删除后，让右子树顶替当前位置。
	 */
	if (!root->left) {
		bst_node *right = root->right;	// 记住右子树根节点
		delete root;
		return right;					// 返回右子树根节点
	}

	/*
	 * 情况 2：没有右孩子
	 *
	 * 说明只有左孩子。
	 * 当前节点删除后，让左子树顶替当前位置。
	 */
	if (!root->right) {
		bst_node *left = root->left;	// 记住左子树根节点
		delete root;
		return left;				  // 返回左子树根节点
	}

	/*
	 * 情况 3：左右孩子都存在
	 *
	 * 这是 BST 删除中最关键的一类情况。
	 * 不能直接粗暴删除当前节点，否则左右两棵子树的连接关系会丢失。
	 *
	 * 当前实现采用“后继替换”策略：
	 *
	 * 第一步：
	 *   在右子树中找到最小节点，它就是当前节点的中序后继。
	 *
	 * 第二步：
	 *   用后继节点的 key 覆盖当前节点的 key。
	 *   注意，这一步只是值替换，不是把整个节点物理搬过来。
	 *
	 * 第三步：
	 *   再到右子树中删除“原来的后继节点”。
	 *
	 * 为什么这样做合法？
	 * 因为后继节点是“比当前节点大的最小值”，
	 * 完成后继的移除后，整棵树重新满足严格中序有序性。
	 * 仅复制 key 的中间状态有两个相同键，不能让外部读者观察。
	 */
	succ = bst_find_min(root->right);
	root->key = succ->key;
	root->right = bst_erase(root->right, succ->key);

	return root;
}

/*
 * 中序遍历打印
 *
 * 参数：
 * root - 当前子树根节点
 *
 * 说明：
 * 对 BST 进行中序遍历，输出结果应当是严格递增的有序序列。
 * 这也是验证 BST 性质最直接的方法之一。
 */
void bst_inorder_print(const bst_node *root)
{
	if (!root)
		return;

	bst_inorder_print(root->left);
	std::cout << root->key << ' ';
	bst_inorder_print(root->right);
}

/*
 * 递归释放整棵树
 *
 * 参数：
 * root - 当前子树根节点
 *
 * 说明：
 * 采用后序方式释放：
 * 先释放左子树，再释放右子树，最后释放当前节点。
 * 这样可以避免访问已释放节点。
 */
void bst_destroy(bst_node *root)
{
	if (!root)
		return;

	bst_destroy(root->left);
	bst_destroy(root->right);
	delete root;
}

int main()
{
	/*
	 * 用一组固定数据构造示例 BST：
	 *
	 *           8
	 *         /   \
	 *        3     10
	 *       / \      \
	 *      1   6      14
	 *         / \     /
	 *        4   7   13
	 */
	const int initial_keys[] = { 8, 3, 10, 1, 6, 14, 4, 7, 13 };
	bst_node *root = nullptr;
	bst_insert_result insert_result;

	/* 逐个插入初始数据 */
	for (int key : initial_keys) {
		insert_result = bst_insert(root, key);
		if (insert_result != bst_insert_result::ok) {
			std::cout << "构建失败：重复键或内存不足\n";
			bst_destroy(root);
			return 1;
		}
	}

	/* 初始中序结果应为有序序列 */
	std::cout << "初始中序: ";
	bst_inorder_print(root);
	std::cout << '\n';

	/*
	 * 删除 7
	 * 7 是叶子节点，对应“叶子删除”
	 */
	root = bst_erase(root, 7);
	std::cout << "删除叶子节点 7 后: ";
	bst_inorder_print(root);
	std::cout << '\n';

	/*
	 * 删除 14
	 * 14 只有一个左孩子 13，对应“单孩子删除”
	 * 删除后由 13 顶替 14 的位置
	 */
	root = bst_erase(root, 14);
	std::cout << "删除单孩子节点 14 后: ";
	bst_inorder_print(root);
	std::cout << '\n';

	/*
	 * 删除 8
	 * 8 同时有左子树和右子树，对应“双孩子删除”
	 * 当前实现会：
	 * 1. 找到后继 10
	 * 2. 用 10 覆盖 8
	 * 3. 再删除原来的 10
	 */
	root = bst_erase(root, 8);
	std::cout << "删除双孩子节点 8 后: ";
	bst_inorder_print(root);
	std::cout << '\n';

	/* 释放整棵树 */
	bst_destroy(root);
	return 0;
}
