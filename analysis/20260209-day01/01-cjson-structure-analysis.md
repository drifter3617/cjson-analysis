# Day 1: cJSON 数据结构深度分析
- **分析日期**: 2026-02-09
- **分析者**: drifter3617  
- **cJSON版本**: v1.7.19
- **GitHub仓库**: https://github.com/drifter3617/cjson-analysis

## 一、数据结构定位

### 1.1 查找结构定义位置
**操作步骤**：
\`\`\`bash
cd ~/桌面/cJSON
grep -n "typedef struct cJSON" cJSON.h
grep -n "^} cJSON;" cJSON.h
\`\`\`

**实际输出**：
\`\`\`
103:typedef struct cJSON
125:typedef struct cJSON_Hooks
123:} cJSON;
\`\`\`

### 1.2 完整结构定义（带注释版）
\`\`\`c
typedef struct cJSON
{
    /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
    struct cJSON *next;
    struct cJSON *prev;
    
    /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
    struct cJSON *child;

    /* The type of the item, as above. */
    int type;

    /* The item's string, if type==cJSON_String  and type == cJSON_Raw */
    char *valuestring;
    
    /* writing to valueint is DEPRECATED, use cJSON_SetNumberValue instead */
    int valueint;
    
    /* The item's number, if type==cJSON_Number */
    double valuedouble;

    /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
    char *string;
} cJSON;
\`\`\`

## 二、字段详细分析

### 2.1 指针字段分析
#### next 和 prev 指针
- **功能**：构成双向链表，连接兄弟节点
- **优势**：
  - 支持双向遍历（前向和后向）
  - O(1)时间复杂度的节点插入和删除
  - 相比数组，动态扩展更灵活

#### child 指针
- **功能**：指向子节点链表的头节点
- **应用场景**：
  - 对象(Object)：child指向第一个键值对
  - 数组(Array)：child指向第一个元素
  - 简单值：child为NULL

### 2.2 类型系统
- **type字段**：使用整型存储类型信息
- **位掩码设计**：
  ```c
  #define cJSON_String (1 << 4)  // 16
  #define cJSON_Array  (1 << 5)  // 32
  #define cJSON_Object (1 << 6)  // 64
优势：支持类型组合检查：if (type & cJSON_String)
扩展性强：新增类型只需定义新掩码
内存高效：单个int存储所有类型信息
当前设计：char *valuestring;   // 字符串
int valueint;        // 整数（已弃用）
double valuedouble;  // 浮点数
并未使用union,可能是为了使代码更加简洁高效，具备更强的兼容性。
键名储存：
string--存储JSON对象，仅当节点是对象的子节点时有效，数组元素string字段为NULL
字段             大小(字节)  累计
--------------------------------
next (指针)       8           8
prev (指针)       8           16  
child (指针)      8           24
type (int)        4           28
对齐填充          4           32
valuestring (指针)8           40
valueint (int)    4           44
对齐填充          4           48
valuedouble       8           56
string (指针)     8           64
--------------------------------
总计：64字节/节点
设计模式识别：组合模式(Composite)：树形结构表示JSON层次

迭代器模式(Iterator)：双向链表支持遍历

访问者模式(Visitor)：根据type字段选择处理逻辑
采用树状+双链表的复合结构，兼顾层次和遍历。位掩码系统灵活二高校。
