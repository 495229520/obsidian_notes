(() => {
  'use strict';

  const p = (id, title, topic, slug, rank, frequency) => ({id, title, topic, slug, rank, frequency});
  const TOPICS = {
    3:{title:'高精度',items:[
      p('415','字符串相加','加法','add-strings',27,241),
      p('2','两数相加','链表进位','add-two-numbers',51,133),
      p('43','字符串相乘','乘法','multiply-strings',53,131),
      p('445','两数相加 II','高位在前','add-two-numbers-ii',148,39),
      p('67','二进制求和','加法','add-binary'),
      p('989','数组形式的整数加法','加法','add-to-array-form-of-integer'),
      p('66','加一','末位进位','plus-one')
    ]},
    4:{title:'前缀和与差分',items:[
      p('560','和为 K 的子数组','前缀和 + 哈希','subarray-sum-equals-k',87,78),
      p('525','连续数组','前缀和 + 哈希','contiguous-array',264,16),
      p('523','连续的子数组和','前缀和取模','continuous-subarray-sum',274,15),
      p('238','除自身以外数组的乘积','前后缀积','product-of-array-except-self',277,15),
      p('304','二维区域和检索','二维前缀和','range-sum-query-2d-immutable'),
      p('974','和可被 K 整除的子数组','前缀和取模','subarray-sums-divisible-by-k'),
      p('1109','航班预订统计','差分','corporate-flight-bookings'),
      p('1094','拼车','差分','car-pooling')
    ]},
    5:{title:'排序',items:[
      p('912','排序数组','快排 / 归并 / 堆排','sort-an-array',8,354),
      p('88','合并两个有序数组','归并','merge-sorted-array',17,294),
      p('56','合并区间','排序','merge-intervals',28,239),
      p('148','排序链表','归并排序','sort-list',45,147),
      p('179','最大数','自定义排序','largest-number',80,83),
      p('75','颜色分类','三路快排','sort-colors',121,49),
      p('164','最大间距','基数 / 桶排序','maximum-gap',300,13),
      p('315','计算右侧小于当前元素的个数','归并计数','count-of-smaller-numbers-after-self'),
      p('493','翻转对','归并计数','reverse-pairs')
    ]},
    6:{title:'二分查找',items:[
      p('33','搜索旋转排序数组','旋转数组','search-in-rotated-sorted-array',13,309),
      p('4','寻找两个正序数组的中位数','划分二分','median-of-two-sorted-arrays',37,171),
      p('704','二分查找','基础二分','binary-search',42,150),
      p('69','x 的平方根','答案二分','sqrtx',46,146),
      p('34','查找元素的首尾位置','边界二分','find-first-and-last-position-of-element-in-sorted-array',60,102),
      p('162','寻找峰值','性质二分','find-peak-element',85,80),
      p('153','寻找旋转数组最小值','旋转数组','find-minimum-in-rotated-sorted-array',106,58),
      p('410','分割数组的最大值','答案二分','split-array-largest-sum',238,19),
      p('875','爱吃香蕉的珂珂','答案二分','koko-eating-bananas'),
      p('1011','在 D 天内送达包裹','答案二分','capacity-to-ship-packages-within-d-days')
    ]},
    7:{title:'双指针与滑动窗口',items:[
      p('3','无重复字符的最长子串','滑动窗口','longest-substring-without-repeating-characters',1,1151),
      p('15','三数之和','对撞指针','3sum',6,478),
      p('42','接雨水','双指针','trapping-rain-water',31,197),
      p('76','最小覆盖子串','滑动窗口','minimum-window-substring',50,133),
      p('209','长度最小的子数组','滑动窗口','minimum-size-subarray-sum',89,73),
      p('283','移动零','同向双指针','move-zeroes',99,67),
      p('11','盛最多水的容器','对撞指针','container-with-most-water',110,56),
      p('167','两数之和 II','对撞指针','two-sum-ii-input-array-is-sorted',260,17),
      p('424','替换后的最长重复字符','滑动窗口','longest-repeating-character-replacement'),
      p('713','乘积小于 K 的子数组','滑动窗口','subarray-product-less-than-k')
    ]},
    8:{title:'哈希表与计数',items:[
      p('1','两数之和','补数哈希','two-sum',14,302),
      p('128','最长连续序列','哈希集合','longest-consecutive-sequence',70,92),
      p('347','前 K 个高频元素','计数哈希','top-k-frequent-elements',138,43),
      p('349','两个数组的交集','哈希集合','intersection-of-two-arrays',201,24),
      p('242','有效的字母异位词','字符计数','valid-anagram',241,18),
      p('49','字母异位词分组','签名归桶','group-anagrams',251,17),
      p('706','设计哈希映射','哈希结构','design-hashmap',278,15),
      p('36','有效的数独','状态哈希','valid-sudoku'),
      p('454','四数相加 II','分组计数','4sum-ii'),
      p('205','同构字符串','双向映射','isomorphic-strings')
    ]},
    9:{title:'链表技巧',items:[
      p('206','反转链表','三指针','reverse-linked-list',3,744),
      p('25','K 个一组翻转链表','分组反转','reverse-nodes-in-k-group',5,521),
      p('21','合并两个有序链表','虚拟头节点','merge-two-sorted-lists',10,330),
      p('92','反转链表 II','区间反转','reverse-linked-list-ii',19,269),
      p('143','重排链表','中点 + 反转','reorder-list',25,253),
      p('141','环形链表','快慢指针','linked-list-cycle',26,250),
      p('160','相交链表','双指针','intersection-of-two-linked-lists',29,201),
      p('19','删除倒数第 N 个节点','快慢指针','remove-nth-node-from-end-of-list',36,182),
      p('142','环形链表 II','Floyd 判圈','linked-list-cycle-ii',38,170),
      p('138','复制带随机指针的链表','映射 / 穿插','copy-list-with-random-pointer',107,58),
      p('147','对链表进行插入排序','插入排序','insertion-sort-list'),
      p('430','扁平化多级双向链表','指针重连','flatten-a-multilevel-doubly-linked-list')
    ]},
    10:{title:'栈与单调结构',items:[
      p('20','有效的括号','普通栈','valid-parentheses',16,295),
      p('239','滑动窗口最大值','单调队列','sliding-window-maximum',41,152),
      p('32','最长有效括号','栈 / DP','longest-valid-parentheses',44,148),
      p('394','字符串解码','辅助栈','decode-string',62,100),
      p('155','最小栈','辅助栈','min-stack',64,99),
      p('739','每日温度','单调栈','daily-temperatures',102,62),
      p('84','柱状图中最大的矩形','单调栈','largest-rectangle-in-histogram',189,26),
      p('503','下一个更大元素 II','单调栈','next-greater-element-ii',221,21),
      p('150','逆波兰表达式求值','普通栈','evaluate-reverse-polish-notation',263,16),
      p('735','行星碰撞','模拟栈','asteroid-collision',298,13),
      p('496','下一个更大元素 I','单调栈','next-greater-element-i'),
      p('901','股票价格跨度','单调栈','online-stock-span')
    ]},
    11:{title:'堆与优先队列',items:[
      p('215','数组中的第 K 个最大元素','Top K','kth-largest-element-in-an-array',4,597),
      p('23','合并 K 个升序链表','多路归并','merge-k-sorted-lists',23,256),
      p('347','前 K 个高频元素','Top K','top-k-frequent-elements',138,43),
      p('295','数据流的中位数','双堆','find-median-from-data-stream',157,36),
      p('264','丑数 II','多路归并','ugly-number-ii',191,25),
      p('692','前 K 个高频单词','Top K','top-k-frequent-words',195,24),
      p('378','有序矩阵中第 K 小的元素','小根堆','kth-smallest-element-in-a-sorted-matrix',219,21),
      p('703','数据流中的第 K 大元素','固定小根堆','kth-largest-element-in-a-stream'),
      p('973','最接近原点的 K 个点','Top K','k-closest-points-to-origin'),
      p('1046','最后一块石头的重量','大根堆','last-stone-weight')
    ]},
    12:{title:'二叉树与遍历',items:[
      p('102','二叉树的层序遍历','BFS','binary-tree-level-order-traversal',11,328),
      p('103','二叉树的锯齿形层序遍历','BFS','binary-tree-zigzag-level-order-traversal',21,266),
      p('236','二叉树的最近公共祖先','后序 DFS','lowest-common-ancestor-of-a-binary-tree',22,265),
      p('124','二叉树中的最大路径和','后序 DFS','binary-tree-maximum-path-sum',35,183),
      p('199','二叉树的右视图','BFS / DFS','binary-tree-right-side-view',40,161),
      p('94','二叉树的中序遍历','中序 DFS','binary-tree-inorder-traversal',47,144),
      p('105','前序与中序构造二叉树','递归','construct-binary-tree-from-preorder-and-inorder-traversal',56,114),
      p('104','二叉树的最大深度','DFS / BFS','maximum-depth-of-binary-tree',72,91),
      p('98','验证二叉搜索树','中序 DFS','validate-binary-search-tree',78,84),
      p('543','二叉树的直径','后序 DFS','diameter-of-binary-tree',84,81),
      p('297','二叉树的序列化与反序列化','遍历编码','serialize-and-deserialize-binary-tree',105,59),
      p('1448','统计二叉树中好节点的数目','路径 DFS','count-good-nodes-in-binary-tree'),
      p('1161','最大层内元素和','分层 BFS','maximum-level-sum-of-a-binary-tree')
    ]},
    13:{title:'红黑树与平衡树应用',items:[
      p('239','滑动窗口最大值','multiset 可选解','sliding-window-maximum',41,152),
      p('295','数据流的中位数','multiset 可选解','find-median-from-data-stream',157,36),
      p('220','存在重复元素 III','有序集合','contains-duplicate-iii'),
      p('480','滑动窗口中位数','有序多重集合','sliding-window-median'),
      p('352','将数据流变为多个不相交区间','有序映射','data-stream-as-disjoint-intervals'),
      p('729','我的日程安排表 I','有序映射','my-calendar-i'),
      p('715','Range 模块','有序映射','range-module'),
      p('855','考场就座','有序集合','exam-room')
    ]},
    14:{title:'Trie 字典树',items:[
      p('79','单词搜索','网格 DFS 基础','word-search',103,61),
      p('208','实现 Trie','前缀树','implement-trie-prefix-tree',145,40),
      p('212','单词搜索 II','Trie + 回溯','word-search-ii'),
      p('648','单词替换','最短词根','replace-words'),
      p('1268','搜索推荐系统','前缀检索','search-suggestions-system'),
      p('421','数组中两个数的最大异或值','01 Trie','maximum-xor-of-two-numbers-in-an-array'),
      p('676','实现一个魔法字典','Trie 搜索','implement-magic-dictionary'),
      p('211','添加与搜索单词','Trie + 通配','design-add-and-search-words-data-structure'),
      p('720','词典中最长的单词','Trie / 排序','longest-word-in-dictionary')
    ]},
    15:{title:'字符串匹配',items:[
      p('10','正则表达式匹配','模式 DP','regular-expression-matching',144,40),
      p('44','通配符匹配','模式 DP','wildcard-matching',173,31),
      p('1044','最长重复子串','滚动哈希','longest-duplicate-substring',211,22),
      p('459','重复的子字符串','KMP','repeated-substring-pattern',224,21),
      p('28','找出字符串中第一个匹配项','KMP','find-the-index-of-the-first-occurrence-in-a-string'),
      p('686','重复叠加字符串匹配','Rabin-Karp','repeated-string-match'),
      p('214','最短回文串','KMP','shortest-palindrome'),
      p('187','重复的 DNA 序列','滚动哈希','repeated-dna-sequences'),
      p('796','旋转字符串','字符串匹配','rotate-string')
    ]},
    16:{title:'数据结构设计',items:[
      p('146','LRU 缓存','哈希 + 双链表','lru-cache',2,929),
      p('232','用栈实现队列','双栈','implement-queue-using-stacks',48,143),
      p('155','最小栈','双栈','min-stack',64,99),
      p('460','LFU 缓存','哈希 + 频次链表','lfu-cache',112,56),
      p('295','数据流的中位数','双堆','find-median-from-data-stream',157,36),
      p('380','O(1) 插入删除和随机获取','数组 + 哈希','insert-delete-getrandom-o1',220,21),
      p('622','设计循环队列','循环数组','design-circular-queue',244,18),
      p('706','设计哈希映射','哈希桶','design-hashmap',278,15),
      p('355','设计推特','哈希 + 堆','design-twitter'),
      p('432','全 O(1) 的数据结构','哈希 + 双链表','all-oone-data-structure')
    ]},
    17:{title:'贪心',items:[
      p('121','买卖股票的最佳时机','维护最低价','best-time-to-buy-and-sell-stock',18,278),
      p('56','合并区间','区间贪心','merge-intervals',28,239),
      p('122','买卖股票的最佳时机 II','局部收益','best-time-to-buy-and-sell-stock-ii',71,91),
      p('55','跳跃游戏','最远覆盖','jump-game',111,56),
      p('135','分发糖果','双向贪心','candy',135,44),
      p('45','跳跃游戏 II','层次边界','jump-game-ii',153,38),
      p('134','加油站','前缀亏损','gas-station',184,27),
      p('763','划分字母区间','最远边界','partition-labels',214,22),
      p('435','无重叠区间','区间调度','non-overlapping-intervals'),
      p('452','用最少数量的箭引爆气球','区间调度','minimum-number-of-arrows-to-burst-balloons')
    ]},
    18:{title:'回溯',items:[
      p('46','全排列','排列树','permutations',15,296),
      p('93','复原 IP 地址','切割回溯','restore-ip-addresses',34,185),
      p('22','括号生成','合法前缀','generate-parentheses',43,150),
      p('78','子集','子集树','subsets',58,105),
      p('39','组合总和','组合树','combination-sum',65,96),
      p('79','单词搜索','网格回溯','word-search',103,61),
      p('47','全排列 II','排序去重','permutations-ii',108,57),
      p('40','组合总和 II','同层去重','combination-sum-ii',113,55),
      p('51','N 皇后','约束剪枝','n-queens',187,26),
      p('37','解数独','约束剪枝','sudoku-solver',233,20),
      p('131','分割回文串','切割回溯','palindrome-partitioning',281,15),
      p('216','组合总和 III','组合树','combination-sum-iii'),
      p('698','划分为 K 个相等的子集','回溯剪枝','partition-to-k-equal-sum-subsets')
    ]},
    19:{title:'动态规划',items:[
      p('53','最大子数组和','线性 DP','maximum-subarray',7,374),
      p('72','编辑距离','二维 DP','edit-distance',30,200),
      p('322','零钱兑换','完全背包','coin-change',52,132),
      p('70','爬楼梯','线性 DP','climbing-stairs',55,129),
      p('64','最小路径和','网格 DP','minimum-path-sum',67,95),
      p('221','最大正方形','网格 DP','maximal-square',73,88),
      p('152','乘积最大子数组','双状态 DP','maximum-product-subarray',76,85),
      p('198','打家劫舍','线性 DP','house-robber',90,73),
      p('139','单词拆分','划分 DP','word-break',94,69),
      p('91','解码方法','线性 DP','decode-ways',142,42),
      p('279','完全平方数','完全背包','perfect-squares',197,24),
      p('983','最低票价','线性 DP','minimum-cost-for-tickets')
    ]},
    20:{title:'背包问题',items:[
      p('322','零钱兑换','完全背包最小值','coin-change',52,132),
      p('139','单词拆分','完全背包可行性','word-break',94,69),
      p('518','零钱兑换 II','完全背包组合数','coin-change-ii',122,48),
      p('416','分割等和子集','0-1 背包','partition-equal-subset-sum',174,30),
      p('279','完全平方数','完全背包','perfect-squares',197,24),
      p('494','目标和','0-1 背包计数','target-sum',215,22),
      p('377','组合总和 IV','完全背包排列数','combination-sum-iv'),
      p('1049','最后一块石头的重量 II','0-1 背包','last-stone-weight-ii'),
      p('474','一和零','二维 0-1 背包','ones-and-zeroes'),
      p('879','盈利计划','二维计数背包','profitable-schemes')
    ]},
    21:{title:'子序列动态规划',items:[
      p('300','最长递增子序列','LIS','longest-increasing-subsequence',20,267),
      p('72','编辑距离','双序列 DP','edit-distance',30,200),
      p('1143','最长公共子序列','LCS','longest-common-subsequence',32,196),
      p('718','最长重复子数组','连续双序列','maximum-length-of-repeated-subarray',96,68),
      p('516','最长回文子序列','区间 / LCS','longest-palindromic-subsequence',155,37),
      p('673','最长递增子序列的个数','LIS 计数','number-of-longest-increasing-subsequence',183,28),
      p('97','交错字符串','双序列 DP','interleaving-string',177,30),
      p('115','不同的子序列','双序列计数','distinct-subsequences',237,19),
      p('674','最长连续递增序列','连续状态','longest-continuous-increasing-subsequence',259,17),
      p('583','两个字符串的删除操作','LCS','delete-operation-for-two-strings'),
      p('1035','不相交的线','LCS','uncrossed-lines')
    ]},
    22:{title:'区间动态规划',items:[
      p('5','最长回文子串','回文区间','longest-palindromic-substring',9,349),
      p('516','最长回文子序列','区间 DP','longest-palindromic-subsequence',155,37),
      p('647','回文子串','回文区间','palindromic-substrings',196,24),
      p('312','戳气球','最后操作','burst-balloons'),
      p('1000','合并石头的最低成本','区间合并','minimum-cost-to-merge-stones'),
      p('1547','切棍子的最小成本','区间切分','minimum-cost-to-cut-a-stick'),
      p('375','猜数字大小 II','极小化极大','guess-number-higher-or-lower-ii'),
      p('132','分割回文串 II','区间预处理','palindrome-partitioning-ii'),
      p('87','扰乱字符串','区间划分','scramble-string')
    ]},
    23:{title:'树形动态规划',items:[
      p('124','二叉树中的最大路径和','树上贡献','binary-tree-maximum-path-sum',35,183),
      p('543','二叉树的直径','树上贡献','diameter-of-binary-tree',84,81),
      p('337','打家劫舍 III','选 / 不选','house-robber-iii',229,20),
      p('437','路径总和 III','树上前缀和','path-sum-iii',299,13),
      p('968','监控二叉树','三状态 DP','binary-tree-cameras'),
      p('1372','二叉树中的最长交错路径','方向状态','longest-zigzag-path-in-a-binary-tree'),
      p('834','树中距离之和','换根 DP','sum-of-distances-in-tree'),
      p('687','最长同值路径','树上贡献','longest-univalue-path'),
      p('2246','相邻字符不同的最长路径','树上贡献','longest-path-with-different-adjacent-characters')
    ]},
    24:{title:'图论基础',items:[
      p('200','岛屿数量','DFS 连通分量','number-of-islands',12,327),
      p('207','课程表','拓扑排序','course-schedule',100,66),
      p('695','岛屿的最大面积','DFS 连通分量','max-area-of-island',69,93),
      p('130','被围绕的区域','边界 DFS','surrounded-regions',256,17),
      p('994','腐烂的橘子','多源 BFS','rotting-oranges',275,15),
      p('133','克隆图','图遍历','clone-graph'),
      p('797','所有可能的路径','DAG DFS','all-paths-from-source-to-target'),
      p('841','钥匙和房间','可达性','keys-and-rooms'),
      p('785','判断二分图','图染色','is-graph-bipartite'),
      p('547','省份数量','连通分量','number-of-provinces')
    ]},
    25:{title:'并查集',items:[
      p('200','岛屿数量','并查集可解','number-of-islands',12,327),
      p('128','最长连续序列','并查集可解','longest-consecutive-sequence',70,92),
      p('130','被围绕的区域','并查集可解','surrounded-regions',256,17),
      p('547','省份数量','连通分量','number-of-provinces'),
      p('684','冗余连接','判环','redundant-connection'),
      p('721','账户合并','集合合并','accounts-merge'),
      p('990','等式方程的可满足性','关系合并','satisfiability-of-equality-equations'),
      p('1319','连通网络的操作次数','连通分量','number-of-operations-to-make-network-connected'),
      p('1202','交换字符串中的元素','分组重排','smallest-string-with-swaps'),
      p('399','除法求值','带权并查集','evaluate-division')
    ]},
    26:{title:'图与图算法',items:[
      p('200','岛屿数量','网格 DFS','number-of-islands',12,327),
      p('695','岛屿的最大面积','网格 DFS','max-area-of-island',69,93),
      p('130','被围绕的区域','边界 DFS','surrounded-regions',256,17),
      p('994','腐烂的橘子','多源 BFS','rotting-oranges',275,15),
      p('1584','连接所有点的最小费用','Kruskal / Prim','min-cost-to-connect-all-points'),
      p('684','冗余连接','Kruskal 判环','redundant-connection'),
      p('1489','关键边和伪关键边','最小生成树','find-critical-and-pseudo-critical-edges-in-minimum-spanning-tree'),
      p('1631','最小体力消耗路径','最短路 / MST','path-with-minimum-effort'),
      p('743','网络延迟时间','Dijkstra','network-delay-time')
    ]},
    27:{title:'拓扑排序与 DAG',items:[
      p('207','课程表','拓扑判环','course-schedule',100,66),
      p('329','矩阵中的最长递增路径','DAG DP','longest-increasing-path-in-a-matrix',125,47),
      p('210','课程表 II','拓扑序','course-schedule-ii',167,33),
      p('269','火星词典','建图 + 拓扑','alien-dictionary'),
      p('310','最小高度树','拓扑剥叶','minimum-height-trees'),
      p('802','找到最终的安全状态','反图拓扑','find-eventual-safe-states'),
      p('1203','项目管理','分层拓扑','sort-items-by-groups-respecting-dependencies'),
      p('2050','并行课程 III','DAG DP','parallel-courses-iii'),
      p('851','喧闹和富有','DAG DP','loud-and-rich')
    ]},
    28:{title:'最短路径',items:[
      p('743','网络延迟时间','Dijkstra','network-delay-time'),
      p('787','K 站中转内最便宜的航班','Bellman-Ford','cheapest-flights-within-k-stops'),
      p('1631','最小体力消耗路径','Dijkstra','path-with-minimum-effort'),
      p('1514','概率最大的路径','Dijkstra','path-with-maximum-probability'),
      p('1334','阈值距离内邻居最少的城市','Floyd / Dijkstra','find-the-city-with-the-smallest-number-of-neighbors-at-a-threshold-distance'),
      p('1293','网格中的最短路径','状态 BFS','shortest-path-in-a-grid-with-obstacles-elimination'),
      p('847','访问所有节点的最短路径','状态 BFS','shortest-path-visiting-all-nodes'),
      p('1976','到达目的地的方案数','Dijkstra 计数','number-of-ways-to-arrive-at-destination'),
      p('2642','设计可以求最短路径的图类','动态最短路','design-graph-with-shortest-path-calculator')
    ]}
  };

  const script = document.currentScript;
  const topic = TOPICS[Number(script && script.dataset.topic)];
  if (!topic) return;

  const items = [...topic.items].sort((a, b) => {
    if (a.rank && b.rank) return a.rank - b.rank;
    if (a.rank) return -1;
    if (b.rank) return 1;
    return Number(a.id) - Number(b.id);
  });
  const hotCount = items.filter(item => item.rank).length;
  const extraCount = items.length - hotCount;
  const section = document.createElement('section');
  section.className = 'algo-practice';
  section.setAttribute('aria-labelledby', 'algo-practice-title');
  section.innerHTML = `
    <div class="algo-practice__head">
      <div>
        <h2 class="algo-practice__title" id="algo-practice-title">${topic.title} · 相关力扣</h2>
        <p class="algo-practice__source">CodeTop 高频题 Top300 快照 · HOT 后为 frequency · 未收录题目显示“不常考”</p>
      </div>
      <div class="algo-practice__summary">HOT ${hotCount} · 补充 ${extraCount}</div>
    </div>
    <div class="algo-practice__list"></div>`;

  const list = section.querySelector('.algo-practice__list');
  items.forEach(item => {
    const link = document.createElement('a');
    const isHot = Boolean(item.rank);
    link.className = 'algo-practice__item';
    link.href = `https://leetcode.cn/problems/${item.slug}/`;
    link.target = '_blank';
    link.rel = 'noopener noreferrer';
    link.title = isHot
      ? `${item.id}. ${item.title} · CodeTop 排名 ${item.rank} · 频率 ${item.frequency}`
      : `${item.id}. ${item.title} · 未进入 CodeTop Top300`;
    link.innerHTML = `<span class="algo-practice__name">${item.id}. ${item.title}</span><span class="algo-practice__topic">${item.topic}</span><span class="algo-practice__freq${isHot ? '' : ' algo-practice__freq--none'}">${isHot ? `HOT · ${item.frequency}` : '不常考'}</span>`;
    list.appendChild(link);
  });

  const host = document.querySelector('.right') || document.querySelector('.wrap') || document.body;
  host.appendChild(section);
})();
