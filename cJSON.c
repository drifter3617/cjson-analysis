
#if !defined(_CRT_SECURE_NO_DEPRECATE) && defined(_MSC_VER)
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif
#if defined(_MSC_VER)
#pragma warning (push)
#pragma warning (disable : 4001)
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <float.h>
//123
#include <ctype.h>    // 用于 isdigit
#include <stdarg.h>   // 用于可变参数
#include <string.h>   // 用于字符串操作
//456
#ifdef ENABLE_LOCALES
#include <locale.h>
#endif

#if defined(_MSC_VER)
#pragma warning (pop)
#endif
#ifdef __GNUC__
#pragma GCC visibility pop
#endif

#include "cJSON.h"

#ifdef true
#undef true
#endif
#define true ((cJSON_bool)1)

#ifdef false
#undef false
#endif
#define false ((cJSON_bool)0)

#ifndef isinf
#define isinf(d) (isnan((d - d)) && !isnan(d))
#endif
#ifndef isnan
#define isnan(d) (d != d)
#endif

#ifndef NAN
#ifdef _WIN32
#define NAN sqrt(-1.0)
#else
#define NAN 0.0/0.0
#endif
#endif
//123

//456

typedef struct {
    const unsigned char *json;
    size_t position;
} error;
static error global_error = { NULL, 0 };

CJSON_PUBLIC(const char *) cJSON_GetErrorPtr(void)
{
    return (const char*) (global_error.json + global_error.position);
}

CJSON_PUBLIC(char *) cJSON_GetStringValue(const cJSON * const item)
{
    if (!cJSON_IsString(item))
    {
        return NULL;
    }

    return item->valuestring;
}

CJSON_PUBLIC(double) cJSON_GetNumberValue(const cJSON * const item)
{
    if (!cJSON_IsNumber(item))
    {
        return (double) NAN;
    }

    return item->valuedouble;
}

#if (CJSON_VERSION_MAJOR != 1) || (CJSON_VERSION_MINOR != 7) || (CJSON_VERSION_PATCH != 19)
    #error cJSON.h and cJSON.c have different versions. Make sure that both have the same.
#endif

CJSON_PUBLIC(const char*) cJSON_Version(void)
{
    static char version[15];
    sprintf(version, "%i.%i.%i", CJSON_VERSION_MAJOR, CJSON_VERSION_MINOR, CJSON_VERSION_PATCH);

    return version;
}

static int case_insensitive_strcmp(const unsigned char *string1, const unsigned char *string2)
{
    if ((string1 == NULL) || (string2 == NULL))
    {
        return 1;
    }

    if (string1 == string2)
    {
        return 0;
    }

    for(; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
    {
        if (*string1 == '\0')
        {
            return 0;
        }
    }

    return tolower(*string1) - tolower(*string2);
}

typedef struct internal_hooks
{
    void *(CJSON_CDECL *allocate)(size_t size);
    void (CJSON_CDECL *deallocate)(void *pointer);
    void *(CJSON_CDECL *reallocate)(void *pointer, size_t size);
} internal_hooks;


#if defined(_MSC_VER)
static void * CJSON_CDECL internal_malloc(size_t size)
{
    return malloc(size);
}
static void CJSON_CDECL internal_free(void *pointer)
{
    free(pointer);
}
static void * CJSON_CDECL internal_realloc(void *pointer, size_t size)
{
    return realloc(pointer, size);
}
#else
#define internal_malloc malloc
#define internal_free free
#define internal_realloc realloc
#endif

#define static_strlen(string_literal) (sizeof(string_literal) - sizeof(""))

static internal_hooks global_hooks = { internal_malloc, internal_free, internal_realloc };
//123

#define get_hooks() (&global_hooks)

/* 指针路径结构体 */
typedef struct pointer_path {
    char **segments;
    size_t count;
    size_t capacity;
    char *error;
} pointer_path;

/* 自定义字符串复制函数（使用hooks）*/
static char* pointer_strdup(const char *str) {
    if (!str) return NULL;
    
    internal_hooks * const hooks = get_hooks();
    size_t len = strlen(str);
    char *copy = (char*)hooks->allocate(len + 1);
    if (!copy) return NULL;
    
    memcpy(copy, str, len + 1);
    return copy;
}

/* 释放指针路径 */
static void free_pointer_path(pointer_path *path) {
    if (!path) return;
    
    internal_hooks * const hooks = get_hooks();
    
    if (path->segments) {
        for (size_t i = 0; i < path->count; i++) {
            if (path->segments[i]) {
                hooks->deallocate(path->segments[i]);
            }
        }
        hooks->deallocate(path->segments);
    }
    if (path->error) {
        hooks->deallocate(path->error);
    }
    hooks->deallocate(path);
}

/* 添加段到路径 */
static cJSON_bool add_segment_to_path(pointer_path *path, const char *segment) {
    if (!path || !segment) return false;
    
    internal_hooks * const hooks = get_hooks();
    
    /* 检查容量 */
    if (path->count >= path->capacity) {
        size_t new_capacity = path->capacity == 0 ? 8 : path->capacity * 2;
        char **new_segments = (char**)hooks->reallocate(path->segments, new_capacity * sizeof(char*));
        if (!new_segments) return false;
        
        path->segments = new_segments;
        path->capacity = new_capacity;
    }
    
    /* 复制段 */
    size_t len = strlen(segment);
    path->segments[path->count] = (char*)hooks->allocate(len + 1);
    if (!path->segments[path->count]) return false;
    
    memcpy(path->segments[path->count], segment, len + 1);
    path->count++;
    
    return true;
}

/* 处理转义字符 */
static void unescape_segment(char *segment) {
    if (!segment) return;
    
    char *read = segment;
    char *write = segment;
    
    while (*read) {
        if (*read == '~') {
            read++;
            if (*read == '0') {
                *write++ = '~';
            } else if (*read == '1') {
                *write++ = '/';
            }
            read++;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

/* 检查数组索引是否有效 */
static cJSON_bool is_valid_array_index(const char *segment) {
    if (!segment || *segment == '\0') return false;
    
    /* RFC 6901 允许 '-' 表示数组末尾 */
    if (*segment == '-') {
        return (strlen(segment) == 1);
    }
    
    /* 检查是否全为数字 */
    for (const char *p = segment; *p; p++) {
        if (!isdigit((unsigned char)*p)) return false;
    }
    
    return true;
}

/* 解析JSON指针 */
static pointer_path* parse_pointer(const char *pointer) {
    internal_hooks * const hooks = get_hooks();
    pointer_path *path = (pointer_path*)hooks->allocate(sizeof(pointer_path));
    if (!path) return NULL;
    
    /* 初始化 */
    memset(path, 0, sizeof(pointer_path));
    
    /* 空指针或空字符串指向根节点 */
    if (!pointer || *pointer == '\0') {
        return path;
    }
    
    /* 必须以 '/' 开头 */
    if (*pointer != '/') {
        const char *err_msg = "JSON pointer must start with '/'";
        size_t len = strlen(err_msg) + 1;
        path->error = (char*)hooks->allocate(len);
        if (path->error) {
            memcpy(path->error, err_msg, len);
        }
        return path;
    }
    
    const char *p = pointer;
    while (*p) {
        if (*p == '/') {
            p++;
            
            const char *segment_start = p;
            size_t segment_len = 0;
            
            while (*p && *p != '/') {
                if (*p == '~') {
                    p++;
                    if (*p == '0' || *p == '1') {
                        segment_len++;
                        p++;
                    } else {
                        const char *err_msg = "Invalid escape sequence in pointer";
                        size_t len = strlen(err_msg) + 1;
                        path->error = (char*)hooks->allocate(len);
                        if (path->error) {
                            memcpy(path->error, err_msg, len);
                        }
                        return path;
                    }
                } else {
                    segment_len++;
                    p++;
                }
            }
            
            /* 提取段 */
            char *segment = (char*)hooks->allocate(segment_len + 1);
            if (!segment) {
                const char *err_msg = "Memory allocation failed";
                size_t len = strlen(err_msg) + 1;
                path->error = (char*)hooks->allocate(len);
                if (path->error) {
                    memcpy(path->error, err_msg, len);
                }
                return path;
            }
            
            memcpy(segment, segment_start, segment_len);
            segment[segment_len] = '\0';
            
            /* 处理转义 */
            unescape_segment(segment);
            
            /* 添加到路径 */
            if (!add_segment_to_path(path, segment)) {
                hooks->deallocate(segment);
                const char *err_msg = "Failed to add segment to path";
                size_t len = strlen(err_msg) + 1;
                path->error = (char*)hooks->allocate(len);
                if (path->error) {
                    memcpy(path->error, err_msg, len);
                }
                return path;
            }
            
            hooks->deallocate(segment);
        } else {
            p++;
        }
    }
    
    return path;
}

/* 通过JSON Pointer获取节点 */
CJSON_PUBLIC(cJSON *) cJSON_GetByPointer(const cJSON * const root, const char * const pointer) {
    if (!root) return NULL;
    
    /* 空指针指向根节点 */
    if (!pointer || *pointer == '\0') {
        return (cJSON*)root;
    }
    
    pointer_path *path = parse_pointer(pointer);
    if (!path || path->error) {
        if (path) free_pointer_path(path);
        return NULL;
    }
    
    const cJSON *current = root;
    
    for (size_t i = 0; i < path->count; i++) {
        const char *segment = path->segments[i];
        
        if (!current) {
            break;
        }
        
        if (cJSON_IsArray(current)) {
            if (!is_valid_array_index(segment)) {
                current = NULL;
                break;
            }
            if (strcmp(segment, "-") == 0) {
                /* '-' 特殊处理，返回数组本身 */
                break;
            }
            int index = atoi(segment);
            current = cJSON_GetArrayItem(current, index);
            
        } else if (cJSON_IsObject(current)) {
            current = cJSON_GetObjectItem(current, segment);
            
        } else {
            current = NULL;
            break;
        }
    }
    
    free_pointer_path(path);
    return (cJSON*)current;
}

/* 通过JSON Pointer获取节点（带错误信息） */
CJSON_PUBLIC(cJSON *) cJSON_GetByPointerEx(const cJSON * const root, 
                                           const char * const pointer, 
                                           char **error) {
    if (error) *error = NULL;
    
    if (!root) {
        if (error) {
            internal_hooks * const hooks = get_hooks();
            const char *err_msg = "Root node is NULL";
            size_t len = strlen(err_msg) + 1;
            *error = (char*)hooks->allocate(len);
            if (*error) memcpy(*error, err_msg, len);
        }
        return NULL;
    }
    
    cJSON *result = cJSON_GetByPointer(root, pointer);
    
    if (!result && error) {
        internal_hooks * const hooks = get_hooks();
        const char *err_msg = "Path not found";
        size_t len = strlen(err_msg) + 1;
        *error = (char*)hooks->allocate(len);
        if (*error) memcpy(*error, err_msg, len);
    }
    
    return result;
}

/* 检查JSON指针路径是否存在 */
CJSON_PUBLIC(cJSON_bool) cJSON_PointerExists(const cJSON * const root, 
                                             const char * const pointer) {
    return (cJSON_GetByPointer(root, pointer) != NULL);
}

/* 转义函数 */
CJSON_PUBLIC(char *) cJSON_EscapePointer(const char * const segment) {
    if (!segment) return NULL;
    
    internal_hooks * const hooks = get_hooks();
    size_t len = strlen(segment);
    size_t escaped_len = len;
    
    /* 计算转义后长度 */
    for (size_t i = 0; i < len; i++) {
        if (segment[i] == '~') escaped_len++;
        if (segment[i] == '/') escaped_len++;
    }
    
    char *escaped = (char*)hooks->allocate(escaped_len + 1);
    if (!escaped) return NULL;
    
    /* 执行转义 */
    char *out = escaped;
    for (size_t i = 0; i < len; i++) {
        if (segment[i] == '~') {
            *out++ = '~';
            *out++ = '0';
        } else if (segment[i] == '/') {
            *out++ = '~';
            *out++ = '1';
        } else {
            *out++ = segment[i];
        }
    }
    *out = '\0';
    
    return escaped;
}

/* 反转义函数 */
CJSON_PUBLIC(char *) cJSON_UnescapePointer(const char * const segment) {
    if (!segment) return NULL;
    
    internal_hooks * const hooks = get_hooks();
    size_t len = strlen(segment);
    char *unescaped = (char*)hooks->allocate(len + 1);
    if (!unescaped) return NULL;
    
    char *out = unescaped;
    for (size_t i = 0; i < len; i++) {
        if (segment[i] == '~' && i + 1 < len) {
            i++;
            if (segment[i] == '0') {
                *out++ = '~';
            } else if (segment[i] == '1') {
                *out++ = '/';
            }
        } else {
            *out++ = segment[i];
        }
    }
    *out = '\0';
    
    return unescaped;
}

/* 构建指针 */
CJSON_PUBLIC(char *) cJSON_BuildPointer(size_t count, ...) {
    internal_hooks * const hooks = get_hooks();
    
    if (count == 0) {
        char *empty = (char*)hooks->allocate(1);
        if (empty) *empty = '\0';
        return empty;
    }
    
    va_list args;
    va_start(args, count);
    
    /* 计算总长度 */
    size_t total_len = 0;
    for (size_t i = 0; i < count; i++) {
        const char *segment = va_arg(args, const char*);
        if (segment) {
            total_len += strlen(segment) + 1;
        }
    }
    va_end(args);
    
    /* 分配内存 */
    char *result = (char*)hooks->allocate(total_len + 1);
    if (!result) return NULL;
    
    /* 构建字符串 */
    char *out = result;
    va_start(args, count);
    for (size_t i = 0; i < count; i++) {
        const char *segment = va_arg(args, const char*);
        *out++ = '/';
        if (segment) {
            size_t len = strlen(segment);
            memcpy(out, segment, len);
            out += len;
        }
    }
    va_end(args);
    *out = '\0';
    
    return result;
}

/* 通过JSON Pointer添加节点 */
CJSON_PUBLIC(cJSON_bool) cJSON_AddByPointer(cJSON * const root, 
                                            const char * const pointer, 
                                            cJSON * const item) {
    if (!root || !pointer || *pointer == '\0' || !item) {
        return false;
    }
    
    pointer_path *path = parse_pointer(pointer);
    if (!path || path->error || path->count == 0) {
        if (path) free_pointer_path(path);
        return false;
    }
    
    cJSON *current = root;
    
    /* 遍历到最后一个段之前 */
    for (size_t i = 0; i < path->count - 1; i++) {
        const char *segment = path->segments[i];
        
        if (!current) {
            free_pointer_path(path);
            return false;
        }
        
        /* 获取或创建下一级节点 */
        cJSON *next = NULL;
        
        if (cJSON_IsArray(current)) {
            if (!is_valid_array_index(segment)) {
                free_pointer_path(path);
                return false;
            }
            if (strcmp(segment, "-") == 0) {
                int size = cJSON_GetArraySize(current);
                next = cJSON_CreateNull();
                cJSON_AddItemToArray(current, next);
            } else {
                int index = atoi(segment);
                next = cJSON_GetArrayItem(current, index);
                if (!next) {
                    next = cJSON_CreateNull();
                    while (cJSON_GetArraySize(current) <= index) {
                        cJSON_AddItemToArray(current, cJSON_CreateNull());
                    }
                    cJSON_ReplaceItemInArray(current, index, next);
                }
            }
        } else if (cJSON_IsObject(current)) {
            next = cJSON_GetObjectItem(current, segment);
            if (!next) {
                next = cJSON_CreateObject();
                cJSON_AddItemToObject(current, segment, next);
            }
        } else {
            free_pointer_path(path);
            return false;
        }
        
        current = next;
    }
    
    /* 在最后一个位置添加目标节点 */
    const char *last_segment = path->segments[path->count - 1];
    cJSON_bool success = false;
    
    if (cJSON_IsArray(current)) {
        if (strcmp(last_segment, "-") == 0) {
            cJSON_AddItemToArray(current, item);
            success = true;
        } else if (is_valid_array_index(last_segment)) {
            int index = atoi(last_segment);
            if (index < cJSON_GetArraySize(current)) {
                cJSON_ReplaceItemInArray(current, index, item);
            } else {
                while (cJSON_GetArraySize(current) < index) {
                    cJSON_AddItemToArray(current, cJSON_CreateNull());
                }
                cJSON_AddItemToArray(current, item);
            }
            success = true;
        }
    } else if (cJSON_IsObject(current)) {
        cJSON_AddItemToObject(current, last_segment, item);
        success = true;
    }
    
    free_pointer_path(path);
    return success;
}

/* 通过JSON Pointer删除节点 */
CJSON_PUBLIC(cJSON_bool) cJSON_DeleteByPointer(cJSON * const root, 
                                               const char * const pointer) {
    if (!root || !pointer || *pointer == '\0') {
        return false;
    }
    
    /* 获取父节点和目标节点 */
    char *parent_pointer = pointer_strdup(pointer);
    if (!parent_pointer) return false;
    
    char *last_slash = strrchr(parent_pointer, '/');
    if (!last_slash) {
        internal_hooks * const hooks = get_hooks();
        hooks->deallocate(parent_pointer);
        return false;
    }
    
    *last_slash = '\0';
    const char *key = last_slash + 1;
    
    cJSON *parent = cJSON_GetByPointer(root, parent_pointer);
    internal_hooks * const hooks = get_hooks();
    hooks->deallocate(parent_pointer);
    
    if (!parent) return false;
    
    if (cJSON_IsArray(parent)) {
        if (is_valid_array_index(key)) {
            int index = atoi(key);
            cJSON_DeleteItemFromArray(parent, index);
            return true;
        }
    } else if (cJSON_IsObject(parent)) {
        cJSON_DeleteItemFromObject(parent, key);
        return true;
    }
    
    return false;
}

/* 通过JSON Pointer更新节点 */
CJSON_PUBLIC(cJSON_bool) cJSON_SetByPointer(cJSON * const root, 
                                            const char * const pointer, 
                                            cJSON * const new_value) {
    if (!root || !pointer || *pointer == '\0' || !new_value) {
        return false;
    }
    
    cJSON *target = cJSON_GetByPointer(root, pointer);
    if (!target) return false;
    
    internal_hooks * const hooks = get_hooks();
    
    /* 替换值 */
    if (target->valuestring) {
        hooks->deallocate(target->valuestring);
        target->valuestring = NULL;
    }
    
    /* 复制新节点的值 */
    target->type = new_value->type;
    
    if (cJSON_IsString(new_value) && new_value->valuestring) {
        target->valuestring = pointer_strdup(new_value->valuestring);
    } else if (cJSON_IsNumber(new_value)) {
        target->valueint = new_value->valueint;
        target->valuedouble = new_value->valuedouble;
    }
    
    return true;
}
//456
/**
 * cJSON字符串复制函数（带内存钩子）
 * 
 * 这是cJSON内部使用的字符串复制函数，它不同于标准的strdup：
 * 1. 使用自定义的内存分配钩子（hooks->allocate）而不是malloc
 * 2. 允许cJSON库在嵌入式系统或特殊内存环境中工作
 * 3. 直接使用memcpy进行内存复制（比strcpy更高效，因为长度已知）
 * 
 * @param string 要复制的源字符串（可以为NULL）
 * @param hooks 内存操作钩子结构体，包含allocate、deallocate等函数指针
 * @return 新分配的字符串副本（需使用hooks->deallocate释放），失败返回NULL
 */
static unsigned char* cJSON_strdup(const unsigned char* string, const internal_hooks * const hooks)
{
    size_t length = 0;
    unsigned char *copy = NULL;

    /* 防御性编程：处理空指针输入
     * 这符合C标准库strdup的行为规范：传入NULL返回NULL
     * 避免了对NULL指针的解引用操作
     */
    if (string == NULL)
    {
        return NULL;
    }

    /**
     * 计算需要分配的内存大小：
     * strlen获取字符串长度（不含结尾的'\0'）
     * sizeof("") == 1，用于添加字符串结束符的空间
     * 这种写法比 +1 更直观地表达了"添加结尾空字符"的意图
     */
    length = strlen((const char*)string) + sizeof("");
    
    /**
     * 使用钩子函数分配内存
     * 这是cJSON可移植性的关键：允许在不同环境中使用自定义内存管理
     * 例如：在嵌入式系统中使用静态内存池，在RTOS中使用专用堆等
     */
    copy = (unsigned char*)hooks->allocate(length);
    if (copy == NULL)
    {
        /* 内存分配失败，遵循cJSON的错误处理模式：返回NULL */
        return NULL;
    }

    /**
     * 使用memcpy进行内存复制
     * 为什么不用strcpy？因为我们已经知道确切的长度
     * memcpy通常比strcpy更高效（不需要逐字节检查'\0'）
     * 复制length字节包含了结尾的'\0'，所以复制后字符串自动终止
     */
    memcpy(copy, string, length);

    return copy;
}

CJSON_PUBLIC(void) cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if (hooks == NULL)
    {
        global_hooks.allocate = malloc;
        global_hooks.deallocate = free;
        global_hooks.reallocate = realloc;
        return;
    }

    global_hooks.allocate = malloc;
    if (hooks->malloc_fn != NULL)
    {
        global_hooks.allocate = hooks->malloc_fn;
    }

    global_hooks.deallocate = free;
    if (hooks->free_fn != NULL)
    {
        global_hooks.deallocate = hooks->free_fn;
    }

    global_hooks.reallocate = NULL;
    if ((global_hooks.allocate == malloc) && (global_hooks.deallocate == free))
    {
        global_hooks.reallocate = realloc;
    }
}

static cJSON *cJSON_New_Item(const internal_hooks * const hooks)
{
    cJSON* node = (cJSON*)hooks->allocate(sizeof(cJSON));
    if (node)
    {
        memset(node, '\0', sizeof(cJSON));
    }

    return node;
}

CJSON_PUBLIC(void) cJSON_Delete(cJSON *item)
{
    cJSON *next = NULL;
    while (item != NULL)
    {
        next = item->next;
        if (!(item->type & cJSON_IsReference) && (item->child != NULL))
        {
            cJSON_Delete(item->child);
        }
        if (!(item->type & cJSON_IsReference) && (item->valuestring != NULL))
        {
            global_hooks.deallocate(item->valuestring);
            item->valuestring = NULL;
        }
        if (!(item->type & cJSON_StringIsConst) && (item->string != NULL))
        {
            global_hooks.deallocate(item->string);
            item->string = NULL;
        }
        global_hooks.deallocate(item);
        item = next;
    }
}

static unsigned char get_decimal_point(void)
{
#ifdef ENABLE_LOCALES
    struct lconv *lconv = localeconv();
    return (unsigned char) lconv->decimal_point[0];
#else
    return '.';
#endif
}

typedef struct
{
    const unsigned char *content;
    size_t length;
    size_t offset;
    size_t depth;
    internal_hooks hooks;
} parse_buffer;

#define can_read(buffer, size) ((buffer != NULL) && (((buffer)->offset + size) <= (buffer)->length))
#define can_access_at_index(buffer, index) ((buffer != NULL) && (((buffer)->offset + index) < (buffer)->length))
#define cannot_access_at_index(buffer, index) (!can_access_at_index(buffer, index))
#define buffer_at_offset(buffer) ((buffer)->content + (buffer)->offset)

static cJSON_bool parse_number(cJSON * const item, parse_buffer * const input_buffer)
{
    double number = 0;
    unsigned char *after_end = NULL;
    unsigned char *number_c_string;
    unsigned char decimal_point = get_decimal_point();
    size_t i = 0;
    size_t number_string_length = 0;
    cJSON_bool has_decimal_point = false;

    if ((input_buffer == NULL) || (input_buffer->content == NULL))
    {
        return false;
    }

    for (i = 0; can_access_at_index(input_buffer, i); i++)
    {
        switch (buffer_at_offset(input_buffer)[i])
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '+':
            case '-':
            case 'e':
            case 'E':
                number_string_length++;
                break;

            case '.':
                number_string_length++;
                has_decimal_point = true;
                break;

            default:
                goto loop_end;
        }
    }
loop_end:
    number_c_string = (unsigned char *) input_buffer->hooks.allocate(number_string_length + 1);
    if (number_c_string == NULL)
    {
        return false;
    }

    memcpy(number_c_string, buffer_at_offset(input_buffer), number_string_length);
    number_c_string[number_string_length] = '\0';

    if (has_decimal_point)
    {
        for (i = 0; i < number_string_length; i++)
        {
            if (number_c_string[i] == '.')
            {
                number_c_string[i] = decimal_point;
            }
        }
    }

    number = strtod((const char*)number_c_string, (char**)&after_end);
    if (number_c_string == after_end)
    {
        input_buffer->hooks.deallocate(number_c_string);
        return false;
    }

    item->valuedouble = number;

    if (number >= INT_MAX)
    {
        item->valueint = INT_MAX;
    }
    else if (number <= (double)INT_MIN)
    {
        item->valueint = INT_MIN;
    }
    else
    {
        item->valueint = (int)number;
    }

    item->type = cJSON_Number;

    input_buffer->offset += (size_t)(after_end - number_c_string);
    input_buffer->hooks.deallocate(number_c_string);
    return true;
}

CJSON_PUBLIC(double) cJSON_SetNumberHelper(cJSON *object, double number)
{
    if (number >= INT_MAX)
    {
        object->valueint = INT_MAX;
    }
    else if (number <= (double)INT_MIN)
    {
        object->valueint = INT_MIN;
    }
    else
    {
        object->valueint = (int)number;
    }

    return object->valuedouble = number;
}

CJSON_PUBLIC(char*) cJSON_SetValuestring(cJSON *object, const char *valuestring)
{
    char *copy = NULL;
    size_t v1_len;
    size_t v2_len;
    if ((object == NULL) || !(object->type & cJSON_String) || (object->type & cJSON_IsReference))
    {
        return NULL;
    }
    if (object->valuestring == NULL || valuestring == NULL)
    {
        return NULL;
    }

    v1_len = strlen(valuestring);
    v2_len = strlen(object->valuestring);

    if (v1_len <= v2_len)
    {
        if (!( valuestring + v1_len < object->valuestring || object->valuestring + v2_len < valuestring ))
        {
            return NULL;
        }
        strcpy(object->valuestring, valuestring);
        return object->valuestring;
    }
    copy = (char*) cJSON_strdup((const unsigned char*)valuestring, &global_hooks);
    if (copy == NULL)
    {
        return NULL;
    }
    if (object->valuestring != NULL)
    {
        cJSON_free(object->valuestring);
    }
    object->valuestring = copy;

    return copy;
}

typedef struct
{
    unsigned char *buffer;
    size_t length;
    size_t offset;
    size_t depth;
    cJSON_bool noalloc;
    cJSON_bool format;
    internal_hooks hooks;
} printbuffer;

static unsigned char* ensure(printbuffer * const p, size_t needed)
{
    unsigned char *newbuffer = NULL;
    size_t newsize = 0;

    if ((p == NULL) || (p->buffer == NULL))
    {
        return NULL;
    }

    if ((p->length > 0) && (p->offset >= p->length))
    {
        return NULL;
    }

    if (needed > INT_MAX)
    {
        return NULL;
    }

    needed += p->offset + 1;
    if (needed <= p->length)
    {
        return p->buffer + p->offset;
    }

    if (p->noalloc) {
        return NULL;
    }

    if (needed > (INT_MAX / 2))
    {
        if (needed <= INT_MAX)
        {
            newsize = INT_MAX;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        newsize = needed * 2;
    }

    if (p->hooks.reallocate != NULL)
    {
        newbuffer = (unsigned char*)p->hooks.reallocate(p->buffer, newsize);
        if (newbuffer == NULL)
        {
            p->hooks.deallocate(p->buffer);
            p->length = 0;
            p->buffer = NULL;

            return NULL;
        }
    }
    else
    {
        newbuffer = (unsigned char*)p->hooks.allocate(newsize);
        if (!newbuffer)
        {
            p->hooks.deallocate(p->buffer);
            p->length = 0;
            p->buffer = NULL;

            return NULL;
        }

        memcpy(newbuffer, p->buffer, p->offset + 1);
        p->hooks.deallocate(p->buffer);
    }
    p->length = newsize;
    p->buffer = newbuffer;

    return newbuffer + p->offset;
}

static void update_offset(printbuffer * const buffer)
{
    const unsigned char *buffer_pointer = NULL;
    if ((buffer == NULL) || (buffer->buffer == NULL))
    {
        return;
    }
    buffer_pointer = buffer->buffer + buffer->offset;

    buffer->offset += strlen((const char*)buffer_pointer);
}

static cJSON_bool compare_double(double a, double b)
{
    double maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (fabs(a - b) <= maxVal * DBL_EPSILON);
}

static cJSON_bool print_number(const cJSON * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    double d = item->valuedouble;
    int length = 0;
    size_t i = 0;
    unsigned char number_buffer[26] = {0};
    unsigned char decimal_point = get_decimal_point();
    double test = 0.0;

    if (output_buffer == NULL)
    {
        return false;
    }

    if (isnan(d) || isinf(d))
    {
        length = sprintf((char*)number_buffer, "null");
    }
    else if(d == (double)item->valueint)
    {
        length = sprintf((char*)number_buffer, "%d", item->valueint);
    }
    else
    {
        length = sprintf((char*)number_buffer, "%1.15g", d);

        if ((sscanf((char*)number_buffer, "%lg", &test) != 1) || !compare_double((double)test, d))
        {
            length = sprintf((char*)number_buffer, "%1.17g", d);
        }
    }

    if ((length < 0) || (length > (int)(sizeof(number_buffer) - 1)))
    {
        return false;
    }

    output_pointer = ensure(output_buffer, (size_t)length + sizeof(""));
    if (output_pointer == NULL)
    {
        return false;
    }

    for (i = 0; i < ((size_t)length); i++)
    {
        if (number_buffer[i] == decimal_point)
        {
            output_pointer[i] = '.';
            continue;
        }

        output_pointer[i] = number_buffer[i];
    }
    output_pointer[i] = '\0';

    output_buffer->offset += (size_t)length;

    return true;
}

static unsigned parse_hex4(const unsigned char * const input)
{
    unsigned int h = 0;
    size_t i = 0;

    for (i = 0; i < 4; i++)
    {
        if ((input[i] >= '0') && (input[i] <= '9'))
        {
            h += (unsigned int) input[i] - '0';
        }
        else if ((input[i] >= 'A') && (input[i] <= 'F'))
        {
            h += (unsigned int) 10 + input[i] - 'A';
        }
        else if ((input[i] >= 'a') && (input[i] <= 'f'))
        {
            h += (unsigned int) 10 + input[i] - 'a';
        }
        else
        {
            return 0;
        }

        if (i < 3)
        {
            h = h << 4;
        }
    }

    return h;
}

/**
 * 将JSON中的UTF-16转义序列（\uXXXX）转换为UTF-8编码
 * 
 * 处理两种情况的Unicode字符：
 * 1. 基本多文种平面（BMP）字符：单个\uXXXX（U+0000到U+FFFF）
 * 2. 辅助平面字符：需要两个\uXXXX组成代理对（Surrogate Pair）
 * 
 * UTF-16代理对规则：
 * - 高代理项（High Surrogate）：U+D800到U+DBFF
 * - 低代理项（Low Surrogate）：U+DC00到U+DFFF
 * - 组合公式：codepoint = 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00)
 * 
 * @param input_pointer 指向"\uXXXX"序列的起始位置（包括反斜杠）
 * @param input_end 输入缓冲区的结束位置，用于边界检查
 * @param output_pointer 输出缓冲区的指针，函数会在此位置写入UTF-8字节序列并更新指针
 * @return 消耗的输入字节数（6表示单个\uXXXX，12表示代理对），失败返回0
 */
static unsigned char utf16_literal_to_utf8(const unsigned char * const input_pointer, 
                                           const unsigned char * const input_end, 
                                           unsigned char **output_pointer)
{
    long unsigned int codepoint = 0;      /* 最终的Unicode码点 */
    unsigned int first_code = 0;           /* 第一个\uXXXX解析出的数值 */
    const unsigned char *first_sequence = input_pointer;
    unsigned char utf8_length = 0;         /* UTF-8编码后的字节数 */
    unsigned char utf8_position = 0;       
    unsigned char sequence_length = 0;     /* 消耗的输入字节数（6或12） */
    unsigned char first_byte_mark = 0;      /* UTF-8首字节的标记位 */

    /* 至少需要6个字符：\ u X X X X */
    if ((input_end - first_sequence) < 6)
    {
        goto fail;
    }

    /* 解析第一个\uXXXX为16位数值 */
    first_code = parse_hex4(first_sequence + 2);

    /* 检查是否为孤立的低代理项（不能单独出现） */
    if (((first_code >= 0xDC00) && (first_code <= 0xDFFF)))
    {
        goto fail;
    }

    /* 判断是否为代理对的高代理项 */
    if ((first_code >= 0xD800) && (first_code <= 0xDBFF))
    {
        const unsigned char *second_sequence = first_sequence + 6;
        unsigned int second_code = 0;
        sequence_length = 12;  /* 代理对消耗12个字节：\uXXXX\uXXXX */

        /* 检查是否有足够的空间存放第二个\uXXXX */
        if ((input_end - second_sequence) < 6)
        {
            goto fail;
        }

        /* 验证第二个序列的格式必须是\uXXXX */
        if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u'))
        {
            goto fail;
        }

        /* 解析第二个\uXXXX */
        second_code = parse_hex4(second_sequence + 2);
        
        /* 验证第二个必须是有效的低代理项 */
        if ((second_code < 0xDC00) || (second_code > 0xDFFF))
        {
            goto fail;
        }

        /**
         * 将代理对组合成完整的Unicode码点
         * 公式：code = 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00)
         * 其中 first_code & 0x3FF 相当于 (first_code - 0xD800)
         */
        codepoint = 0x10000 + (((first_code & 0x3FF) << 10) | (second_code & 0x3FF));
    }
    else
    {
        /* 单个\uXXXX，直接使用解析出的数值 */
        sequence_length = 6;
        codepoint = first_code;
    }

    /**
     * 根据Unicode码点范围确定UTF-8编码长度和首字节标记
     * UTF-8编码规则：
     * U+0000 - U+007F:   0xxxxxxx (1字节)
     * U+0080 - U+07FF:   110xxxxx 10xxxxxx (2字节)
     * U+0800 - U+FFFF:   1110xxxx 10xxxxxx 10xxxxxx (3字节)
     * U+10000 - U+10FFFF:11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (4字节)
     */
    if (codepoint < 0x80)
    {
        utf8_length = 1;
        /* first_byte_mark保持0，ASCII无需标记 */
    }
    else if (codepoint < 0x800)
    {
        utf8_length = 2;
        first_byte_mark = 0xC0;  /* 110xxxxx */
    }
    else if (codepoint < 0x10000)
    {
        utf8_length = 3;
        first_byte_mark = 0xE0;  /* 1110xxxx */
    }
    else if (codepoint <= 0x10FFFF)  /* Unicode最大码点 */
    {
        utf8_length = 4;
        first_byte_mark = 0xF0;  /* 11110xxx */
    }
    else
    {
        goto fail;  /* 超出Unicode范围 */
    }

    /**
     * 从后向前填充UTF-8字节序列
     * 每个后续字节都是10xxxxxx格式
     * 最后处理首字节（包含长度标记）
     */
    for (utf8_position = (unsigned char)(utf8_length - 1); utf8_position > 0; utf8_position--)
    {
        /* 取低6位，加上10xxxxxx标记 */
        (*output_pointer)[utf8_position] = (unsigned char)((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;  /* 准备处理下6位 */
    }
    
    /* 处理首字节 */
    if (utf8_length > 1)
    {
        /* 组合长度标记和剩余的有效位 */
        (*output_pointer)[0] = (unsigned char)((codepoint | first_byte_mark) & 0xFF);
    }
    else
    {
        /* ASCII字符直接使用 */
        (*output_pointer)[0] = (unsigned char)(codepoint & 0x7F);
    }

    /* 更新输出指针位置 */
    *output_pointer += utf8_length;

    return sequence_length;

fail:
    return 0;  /* 解析失败 */
}

/**
 * 解析JSON字符串并将其转换为C字符串
 * 
 * 函数处理流程：
 * 1. 验证字符串起始字符必须是双引号
 * 2. 第一遍扫描：找到结束双引号，同时统计转义字符数量以计算解码后长度
 * 3. 第二遍扫描：实际进行转义序列的解码
 * 
 * @param item 待填充的cJSON对象，解析成功后会设置type为cJSON_String并填充valuestring
 * @param input_buffer 输入缓冲区，包含当前解析位置和剩余数据
 * @return 解析成功返回true，失败返回false
 */
static cJSON_bool parse_string(cJSON * const item, parse_buffer * const input_buffer)
{
    const unsigned char *input_pointer = buffer_at_offset(input_buffer) + 1;
    const unsigned char *input_end = buffer_at_offset(input_buffer) + 1;
    unsigned char *output_pointer = NULL;
    unsigned char *output = NULL;

    /* 检查字符串必须以双引号开头 */
    if (buffer_at_offset(input_buffer)[0] != '\"')
    {
        goto fail;
    }

    /**
     * 第一阶段：扫描整个字符串，确定解码后的长度
     * 跳过转义字符计算实际字符数，为内存分配做准备
     */
    {
        size_t allocation_length = 0;
        size_t skipped_bytes = 0;  /* 转义字符占用的额外字节数 */
        
        /* 寻找结束双引号，同时处理转义序列 */
        while (((size_t)(input_end - input_buffer->content) < input_buffer->length) && (*input_end != '\"'))
        {
            if (input_end[0] == '\\')
            {
                /* 检查反斜杠后是否还有字符 */
                if ((size_t)(input_end + 1 - input_buffer->content) >= input_buffer->length)
                {
                    goto fail;  /* 字符串截断，非法JSON */
                }
                skipped_bytes++;  /* 反斜杠本身不计入最终字符串长度 */
                input_end++;      /* 跳过反斜杠，下一轮循环处理转义字符 */
            }
            input_end++;
        }
        
        /* 验证是否找到结束双引号 */
        if (((size_t)(input_end - input_buffer->content) >= input_buffer->length) || (*input_end != '\"'))
        {
            goto fail;  /* 未找到结束双引号，字符串不完整 */
        }

        /**
         * 计算解码后的字符串长度：
         * (结束位置 - 起始位置) 是原始字符串长度（包括双引号）
         * 减去1个起始双引号，再减去转义字符数，得到解码后的长度
         * 最后+1用于结尾的'\0'
         */
        allocation_length = (size_t) (input_end - buffer_at_offset(input_buffer)) - skipped_bytes;
        output = (unsigned char*)input_buffer->hooks.allocate(allocation_length + sizeof(""));
        if (output == NULL)
        {
            goto fail;  /* 内存分配失败 */
        }
    }

    /**
     * 第二阶段：实际解码
     * 遍历原始字符串，处理转义序列，将解码后的字符写入输出缓冲区
     */
    output_pointer = output;
    while (input_pointer < input_end)
    {
        if (*input_pointer != '\\')
        {
            /* 普通字符直接复制 */
            *output_pointer++ = *input_pointer++;
        }
        else
        {
            /* 处理转义序列：\x 格式 */
            unsigned char sequence_length = 2;
            if ((input_end - input_pointer) < 1)
            {
                goto fail;  /* 转义序列不完整 */
            }

            switch (input_pointer[1])
            {
                /* JSON标准转义序列 */
                case 'b':  /* 退格 */
                    *output_pointer++ = '\b';
                    break;
                case 'f':  /* 换页 */
                    *output_pointer++ = '\f';
                    break;
                case 'n':  /* 换行 */
                    *output_pointer++ = '\n';
                    break;
                case 'r':  /* 回车 */
                    *output_pointer++ = '\r';
                    break;
                case 't':  /* 制表符 */
                    *output_pointer++ = '\t';
                    break;
                case '\"':  /* 双引号 */
                case '\\':  /* 反斜杠 */
                case '/':   /* 正斜杠（非必须但允许转义） */
                    *output_pointer++ = input_pointer[1];
                    break;

                case 'u':  /* Unicode转义：\uXXXX */
                    /**
                     * 处理UTF-16代理对并转换为UTF-8
                     * 返回实际消耗的字节数（可能大于2，处理代理对时）
                     * 返回0表示解析失败
                     */
                    sequence_length = utf16_literal_to_utf8(input_pointer, input_end, &output_pointer);
                    if (sequence_length == 0)
                    {
                        goto fail;  /* 无效的Unicode转义序列 */
                    }
                    break;

                default:
                    goto fail;  /* 非法的转义序列 */
            }
            /* 跳过已处理的转义序列（包括反斜杠和转义字符） */
            input_pointer += sequence_length;
        }
    }

    /* 添加字符串结束符 */
    *output_pointer = '\0';

    /* 填充cJSON对象 */
    item->type = cJSON_String;
    item->valuestring = (char*)output;

    /* 更新解析位置到结束双引号之后 */
    input_buffer->offset = (size_t) (input_end - input_buffer->content);
    input_buffer->offset++;

    return true;

fail:
    /* 错误处理：清理已分配的内存并回滚解析位置 */
    if (output != NULL)
    {
        input_buffer->hooks.deallocate(output);
        output = NULL;
    }

    if (input_pointer != NULL)
    {
        /* 回滚到错误发生的位置，便于上层进行错误恢复 */
        input_buffer->offset = (size_t)(input_pointer - input_buffer->content);
    }

    return false;
}

/**
 * 将JSON字符串转义并写入输出缓冲区
 * 
 * 函数处理流程：
 * 1. 处理边界情况（空缓冲区、空字符串）
 * 2. 第一遍扫描：统计需要转义的字符数量，计算所需缓冲区大小
 * 3. 第二遍扫描：实际进行字符转义和复制
 * 
 * @param input 输入的字符串指针（UTF-8编码）
 * @param output_buffer 输出缓冲区，包含当前写入位置和剩余空间
 * @return 成功返回true，失败返回false
 */
static cJSON_bool print_string_ptr(const unsigned char * const input, printbuffer * const output_buffer)
{
    const unsigned char *input_pointer = NULL;
    unsigned char *output = NULL;
    unsigned char *output_pointer = NULL;
    size_t output_length = 0;
    size_t escape_characters = 0;

    /* 防御性编程：检查输出缓冲区有效性 */
    if (output_buffer == NULL)
    {
        return false;
    }

    /* 处理空字符串（NULL指针）的情况，输出JSON空字符串字面量 "" */
    if (input == NULL)
    {
        output = ensure(output_buffer, sizeof("\"\""));
        if (output == NULL)
        {
            return false;
        }
        strcpy((char*)output, "\"\"");

        return true;
    }

    /* 第一遍扫描：统计需要转义的字符数量，用于精确分配内存
     * JSON字符串转义规则：
     * 1. 双引号、反斜杠、控制字符（\b,\f,\n,\r,\t）转义为2个字符
     * 2. 其他控制字符（<32）转义为\uXXXX格式，占用6个字符（\u + 4位十六进制）
     */
    for (input_pointer = input; *input_pointer; input_pointer++)
    {
        switch (*input_pointer)
        {
            case '\"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                escape_characters++;  /* 这些字符转义后长度+1（变为2个字符） */
                break;
            default:
                if (*input_pointer < 32)  /* ASCII控制字符，需转义为\uXXXX */
                {
                    escape_characters += 5;  /* 原字符1字节 + 5字节转义 = 6字节 */
                }
                break;
        }
    }
    
    /* 计算最终字符串长度：
     * output_length = 原始字符数 + 转义增加的字符数
     * 最后加上2个双引号和1个结尾\0
     */
    output_length = (size_t)(input_pointer - input) + escape_characters;

    /* 分配足够的内存空间（包括两端双引号和结尾\0） */
    output = ensure(output_buffer, output_length + sizeof("\"\""));
    if (output == NULL)
    {
        return false;
    }

    /* 优化路径：没有需要转义的字符，直接复制并添加双引号 */
    if (escape_characters == 0)
    {
        output[0] = '\"';
        memcpy(output + 1, input, output_length);
        output[output_length + 1] = '\"';
        output[output_length + 2] = '\0';

        return true;
    }

    /* 常规路径：处理需要转义的字符
     * 逐字符处理，根据JSON规范进行转义
     */
    output[0] = '\"';
    output_pointer = output + 1;
    for (input_pointer = input; *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
    {
        /* 普通字符：直接复制（ASCII可打印字符且不是特殊字符） */
        if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
        {
            *output_pointer = *input_pointer;
        }
        else
        {
            /* 需要转义的字符：先加反斜杠前缀 */
            *output_pointer++ = '\\';
            switch (*input_pointer)
            {
                /* JSON标准转义序列 */
                case '\\':
                    *output_pointer = '\\';
                    break;
                case '\"':
                    *output_pointer = '\"';
                    break;
                case '\b':
                    *output_pointer = 'b';
                    break;
                case '\f':
                    *output_pointer = 'f';
                    break;
                case '\n':
                    *output_pointer = 'n';
                    break;
                case '\r':
                    *output_pointer = 'r';
                    break;
                case '\t':
                    *output_pointer = 't';
                    break;
                default:
                    /* Unicode转义序列：\uXXXX格式，处理所有其他控制字符
                     * 确保JSON字符串中不出现原始控制字符
                     */
                    sprintf((char*)output_pointer, "u%04x", *input_pointer);
                    output_pointer += 4;
                    break;
            }
        }
    }
    
    /* 添加结尾双引号和字符串结束符 */
    output[output_length + 1] = '\"';
    output[output_length + 2] = '\0';

    return true;
}

/**
 * 打印字符串值的函数
 * 1. 这是一个包装函数，直接调用底层的print_string_ptr函数
 * 2. 将item->valuestring强制转换为unsigned char*是因为print_string_ptr
 *    内部需要进行字符编码处理，unsigned char*更适合处理原始字节数据
 * 3. 保持函数签名简单，只接受item和printbuffer，符合上层调用的统一接口
 */
static cJSON_bool print_string(const cJSON * const item, printbuffer * const p)
{
    return print_string_ptr((unsigned char*)item->valuestring, p);
}

/* 解析JSON值的函数声明
 * 为什么声明为static：
 * - 这些函数是cJSON内部实现细节，不对外暴露
 * - 限制作用域在当前编译单元，避免符号冲突
 * - 符合信息隐藏原则，只通过公共API访问
 */
static cJSON_bool parse_value(cJSON * const item, parse_buffer * const input_buffer);

/**
 * 打印JSON值的函数声明
 * 
 * 为什么需要对应的parse/print对：
 * - 对称设计：解析和打印是互逆操作，成对出现便于维护和理解
 * - 递归处理：JSON结构是递归的，解析和打印函数也采用递归实现
 */
static cJSON_bool print_value(const cJSON * const item, printbuffer * const output_buffer);

/* 解析JSON数组的函数声明
 * 
 * 为什么单独分离数组处理：
 * - 数组是JSON的基本数据结构，需要特殊处理方括号[]和逗号,
 * - 分离处理使代码结构清晰，职责单一
 */
static cJSON_bool parse_array(cJSON * const item, parse_buffer * const input_buffer);

/**
 * 打印JSON数组的函数声明
 * 
 * 为什么使用printbuffer而不是直接输出：
 * - printbuffer提供了缓冲机制，减少内存分配次数
 * - 支持预计算长度和实际打印两种模式（通过format和buffer成员控制）
 */
static cJSON_bool print_array(const cJSON * const item, printbuffer * const output_buffer);

/* 解析JSON对象的函数声明
 * 
 * 为什么对象需要单独处理：
 * - 对象有键值对结构，需要处理花括号{}和冒号:
 * - 键必须是字符串，需要额外的字符串解析逻辑
 */
static cJSON_bool parse_object(cJSON * const item, parse_buffer * const input_buffer);

/**
 * 打印JSON对象的函数声明
 * 
 * 为什么所有打印函数都返回cJSON_bool：
 * - 统一返回类型，便于错误处理
 * - true表示成功，false表示失败（如内存不足、无效输入）
 * - 符合cJSON库的一致错误处理风格
 */
static cJSON_bool print_object(const cJSON * const item, printbuffer * const output_buffer);
/**
 * 跳过JSON字符串中的空白字符
 * 
 * 设计缘由：
 * 1. JSON标准(RFC 8259)允许在语法元素间存在空白(空格/制表/换行/回车)
 * 2. 解析器需要跳过这些无意义的空白，定位到真正的JSON内容开始位置
 * 3. 空白字符判断：ASCII码 <= 32 包含了所有标准空白字符
 * 
 * 实现原理：
 * 1. 防御性检查：确保缓冲区有效，避免空指针崩溃
 * 2. 边界处理：如果已到末尾，无需跳过直接返回
 * 3. 循环跳过：连续读取字符直到遇到非空白
 * 4. 越界保护：跳过所有空白后如果刚好到末尾，回退一位防止后续访问越界
 * 
 * @param buffer 解析缓冲区
 * @return 跳过空白后的缓冲区，失败返回NULL
 */
static parse_buffer *buffer_skip_whitespace(parse_buffer * const buffer)
{
    /* 防御性编程：确保缓冲区和内容有效 */
    if ((buffer == NULL) || (buffer->content == NULL))
    {
        return NULL;
    }

    /* 边界处理：已无字符可访问，直接返回 */
    if (cannot_access_at_index(buffer, 0))
    {
        return buffer;
    }

    /* 核心逻辑：跳过所有空白字符（ASCII码 <= 32） */
    while (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] <= 32))
    {
       buffer->offset++;
    }

    /* 越界保护：如果offset指向末尾，回退一位避免后续访问越界 */
    if (buffer->offset == buffer->length)
    {
        buffer->offset--;
    }

    return buffer;
}

/**
 * @brief 跳过UTF-8 BOM (Byte Order Mark) 头部
 * 
 * 该函数用于检测并跳过UTF-8文件的BOM头（EF BB BF）。
 * BOM是Unicode标准中用于标识文本编码的标记，UTF-8的BOM是可选的，
 * 但在某些情况下（如Windows记事本保存的UTF-8文件）会包含BOM头。
 * 
 * @param buffer 指向parse_buffer结构体的指针，包含要处理的数据缓冲区
 * @return parse_buffer* 成功时返回原buffer指针（可能已更新offset），失败时返回NULL
 */
static parse_buffer *skip_utf8_bom(parse_buffer * const buffer)
{
    /* 参数有效性检查：
     * 1. buffer指针不能为NULL
     * 2. buffer的内容不能为NULL
     * 3. 当前偏移量必须为0（只能在开始位置跳过BOM）
     * 如果任何条件不满足，返回NULL表示无法处理
     */
    if ((buffer == NULL) || (buffer->content == NULL) || (buffer->offset != 0))
    {
        return NULL;
    }

    /* 检查BOM头是否存在：
     * 1. can_access_at_index(buffer, 4)：确保缓冲区至少有4个字节可访问
     *    为什么要检查4个字节？因为需要读取前3个字节比较，同时确保不越界
     * 2. strncmp比较前3个字节是否为UTF-8 BOM：\xEF\xBB\xBF
     *    UTF-8 BOM的十六进制表示：EF BB BF
     */
    if (can_access_at_index(buffer, 4) && (strncmp((const char*)buffer_at_offset(buffer), "\xEF\xBB\xBF", 3) == 0))
    {
        /* 如果检测到BOM头，将偏移量向后移动3个字节，
         * 这样后续的数据处理就会跳过BOM，直接处理真正的JSON内容
         */
        buffer->offset += 3;
    }

    /* 返回更新后的buffer指针：
     * - 如果检测到BOM，offset已经增加了3
     * - 如果没有BOM，offset保持为0
     * - 如果参数无效，返回NULL
     */
    return buffer;
}

/**
 * 解析JSON字符串（高级版）
 * 
 * @param value 要解析的JSON字符串（必须以null结尾）
 * @param return_parse_end 输出参数，返回解析结束的位置（用于错误定位）
 * @param require_null_terminated 是否强制检查JSON后没有多余字符
 * @return 解析成功的cJSON对象，失败返回NULL
 * 
 * 说明：
 * - 相比cJSON_Parse()，这个函数提供了更多控制选项
 * - require_null_terminated = true 表示严格模式：JSON后只能有空白字符
 * - 可通过return_parse_end获取解析中断位置，便于调试
 */
CJSON_PUBLIC(cJSON *) cJSON_ParseWithOpts(const char *value, const char **return_parse_end, cJSON_bool require_null_terminated)
{
    size_t buffer_length;

    if (NULL == value)
    {
        return NULL;
    }

    /* 计算缓冲区长度（包含结尾的'\0'），传递给更底层的解析函数 */
    buffer_length = strlen(value) + sizeof("");

    /* 委托给功能更完整的cJSON_ParseWithLengthOpts处理 */
    return cJSON_ParseWithLengthOpts(value, buffer_length, return_parse_end, require_null_terminated);
}

/**
 * 核心JSON解析函数
 * 
 * 设计思想：
 * 1. 单遍扫描(one-pass)解析，时间复杂度O(n)
 * 2. 递归下降解析器，解析和构建同步进行
 * 
 * @param value 要解析的JSON字符串
 * @param buffer_length 字符串长度
 * @param return_parse_end 返回解析结束位置（用于错误定位）
 * @param require_null_terminated 是否需要检查字符串以null结尾
 */
CJSON_PUBLIC(cJSON *) cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length, const char **return_parse_end, cJSON_bool require_null_terminated)
{
    /* parse_buffer封装解析状态：位置指针、深度计数器、内存钩子
     * 这种设计使解析器可重入(reentrant)，支持多线程 */
    parse_buffer buffer = { 0, 0, 0, 0, { 0, 0, 0 } };
    cJSON *item = NULL;

    /* 重置全局错误状态（注意：非线程安全） */
    global_error.json = NULL;
    global_error.position = 0;

    /* 输入验证：快速失败原则，避免无效输入 */
    if (value == NULL || 0 == buffer_length)
    {
        goto fail;
    }

    /* 初始化解析缓冲区 */
    buffer.content = (const unsigned char*)value;
    buffer.length = buffer_length;
    buffer.offset = 0;
    buffer.hooks = global_hooks;

    /* 分配根节点，解析失败时需级联释放整个树 */
    item = cJSON_New_Item(&global_hooks);
    if (item == NULL)
    {
        goto fail;
    }

    /* 递归下降解析入口：
     * 1. 跳过UTF-8 BOM（处理平台编码差异）
     * 2. 跳过空白字符（JSON标准允许任意空白）
     * 3. parse_value根据首字符决定调用哪个具体解析函数 */
    if (!parse_value(item, buffer_skip_whitespace(skip_utf8_bom(&buffer))))
    {
        goto fail;
    }

    /* 严格模式：有效JSON后不能有非空白字符 */
    if (require_null_terminated)
    {
        buffer_skip_whitespace(&buffer);
        if ((buffer.offset >= buffer.length) || buffer_at_offset(&buffer)[0] != '\0')
        {
            goto fail;
        }
    }
    if (return_parse_end)
    {
        *return_parse_end = (const char*)buffer_at_offset(&buffer);
    }

    return item;

fail:
    /* 错误处理：清理已分配资源，记录错误位置 */
    if (item != NULL)
    {
        cJSON_Delete(item);
    }

    if (value != NULL)
    {
        error local_error;
        local_error.json = (const unsigned char*)value;
        local_error.position = 0;

        /* 确定错误位置：优先使用buffer.offset，其次是缓冲区末尾 */
        if (buffer.offset < buffer.length)
        {
            local_error.position = buffer.offset;
        }
        else if (buffer.length > 0)
        {
            local_error.position = buffer.length - 1;
        }

        if (return_parse_end != NULL)
        {
            *return_parse_end = (const char*)local_error.json + local_error.position;
        }

        global_error = local_error;
    }

    return NULL;
}
CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value)
{
    return cJSON_ParseWithOpts(value, 0, 0);
}

CJSON_PUBLIC(cJSON *) cJSON_ParseWithLength(const char *value, size_t buffer_length)
{
    return cJSON_ParseWithLengthOpts(value, buffer_length, 0, 0);
}

#define cjson_min(a, b) (((a) < (b)) ? (a) : (b))

static unsigned char *print(const cJSON * const item, cJSON_bool format, const internal_hooks * const hooks)
{
    static const size_t default_buffer_size = 256;
    printbuffer buffer[1];
    unsigned char *printed = NULL;

    memset(buffer, 0, sizeof(buffer));

    buffer->buffer = (unsigned char*) hooks->allocate(default_buffer_size);
    buffer->length = default_buffer_size;
    buffer->format = format;
    buffer->hooks = *hooks;
    if (buffer->buffer == NULL)
    {
        goto fail;
    }

    if (!print_value(item, buffer))
    {
        goto fail;
    }
    update_offset(buffer);

    if (hooks->reallocate != NULL)
    {
        printed = (unsigned char*) hooks->reallocate(buffer->buffer, buffer->offset + 1);
        if (printed == NULL) {
            goto fail;
        }
        buffer->buffer = NULL;
    }
    else
    {
        printed = (unsigned char*) hooks->allocate(buffer->offset + 1);
        if (printed == NULL)
        {
            goto fail;
        }
        memcpy(printed, buffer->buffer, cjson_min(buffer->length, buffer->offset + 1));
        printed[buffer->offset] = '\0';

        hooks->deallocate(buffer->buffer);
        buffer->buffer = NULL;
    }

    return printed;

fail:
    if (buffer->buffer != NULL)
    {
        hooks->deallocate(buffer->buffer);
        buffer->buffer = NULL;
    }

    if (printed != NULL)
    {
        hooks->deallocate(printed);
        printed = NULL;
    }

    return NULL;
}

CJSON_PUBLIC(char *) cJSON_Print(const cJSON *item)
{
    return (char*)print(item, true, &global_hooks);
}

CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item)
{
    return (char*)print(item, false, &global_hooks);
}

CJSON_PUBLIC(char *) cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt)
{
    printbuffer p = { 0, 0, 0, 0, 0, 0, { 0, 0, 0 } };

    if (prebuffer < 0)
    {
        return NULL;
    }

    p.buffer = (unsigned char*)global_hooks.allocate((size_t)prebuffer);
    if (!p.buffer)
    {
        return NULL;
    }

    p.length = (size_t)prebuffer;
    p.offset = 0;
    p.noalloc = false;
    p.format = fmt;
    p.hooks = global_hooks;

    if (!print_value(item, &p))
    {
        global_hooks.deallocate(p.buffer);
        p.buffer = NULL;
        return NULL;
    }

    return (char*)p.buffer;
}

CJSON_PUBLIC(cJSON_bool) cJSON_PrintPreallocated(cJSON *item, char *buffer, const int length, const cJSON_bool format)
{
    printbuffer p = { 0, 0, 0, 0, 0, 0, { 0, 0, 0 } };

    if ((length < 0) || (buffer == NULL))
    {
        return false;
    }

    p.buffer = (unsigned char*)buffer;
    p.length = (size_t)length;
    p.offset = 0;
    p.noalloc = true;
    p.format = format;
    p.hooks = global_hooks;

    return print_value(item, &p);
}

static cJSON_bool parse_value(cJSON * const item, parse_buffer * const input_buffer)
{
    if ((input_buffer == NULL) || (input_buffer->content == NULL))
    {
        return false;
    }

    if (can_read(input_buffer, 4) && (strncmp((const char*)buffer_at_offset(input_buffer), "null", 4) == 0))
    {
        item->type = cJSON_NULL;
        input_buffer->offset += 4;
        return true;
    }
    if (can_read(input_buffer, 5) && (strncmp((const char*)buffer_at_offset(input_buffer), "false", 5) == 0))
    {
        item->type = cJSON_False;
        input_buffer->offset += 5;
        return true;
    }
    if (can_read(input_buffer, 4) && (strncmp((const char*)buffer_at_offset(input_buffer), "true", 4) == 0))
    {
        item->type = cJSON_True;
        item->valueint = 1;
        input_buffer->offset += 4;
        return true;
    }
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '\"'))
    {
        return parse_string(item, input_buffer);
    }
    if (can_access_at_index(input_buffer, 0) && ((buffer_at_offset(input_buffer)[0] == '-') || ((buffer_at_offset(input_buffer)[0] >= '0') && (buffer_at_offset(input_buffer)[0] <= '9'))))
    {
        return parse_number(item, input_buffer);
    }
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '['))
    {
        return parse_array(item, input_buffer);
    }
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '{'))
    {
        return parse_object(item, input_buffer);
    }

    return false;
}

static cJSON_bool print_value(const cJSON * const item, printbuffer * const output_buffer)
{
    unsigned char *output = NULL;

    if ((item == NULL) || (output_buffer == NULL))
    {
        return false;
    }

    switch ((item->type) & 0xFF)
    {
        case cJSON_NULL:
            output = ensure(output_buffer, 5);
            if (output == NULL)
            {
                return false;
            }
            strcpy((char*)output, "null");
            return true;

        case cJSON_False:
            output = ensure(output_buffer, 6);
            if (output == NULL)
            {
                return false;
            }
            strcpy((char*)output, "false");
            return true;

        case cJSON_True:
            output = ensure(output_buffer, 5);
            if (output == NULL)
            {
                return false;
            }
            strcpy((char*)output, "true");
            return true;

        case cJSON_Number:
            return print_number(item, output_buffer);

        case cJSON_Raw:
        {
            size_t raw_length = 0;
            if (item->valuestring == NULL)
            {
                return false;
            }

            raw_length = strlen(item->valuestring) + sizeof("");
            output = ensure(output_buffer, raw_length);
            if (output == NULL)
            {
                return false;
            }
            memcpy(output, item->valuestring, raw_length);
            return true;
        }

        case cJSON_String:
            return print_string(item, output_buffer);

        case cJSON_Array:
            return print_array(item, output_buffer);

        case cJSON_Object:
            return print_object(item, output_buffer);

        default:
            return false;
    }
}

static cJSON_bool parse_array(cJSON * const item, parse_buffer * const input_buffer)
{
    cJSON *head = NULL;
    cJSON *current_item = NULL;

    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
    {
        return false;
    }
    input_buffer->depth++;

    if (buffer_at_offset(input_buffer)[0] != '[')
    {
        goto fail;
    }

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ']'))
    {
        goto success;
    }

    if (cannot_access_at_index(input_buffer, 0))
    {
        input_buffer->offset--;
        goto fail;
    }

    input_buffer->offset--;
    do
    {
        cJSON *new_item = cJSON_New_Item(&(input_buffer->hooks));
        if (new_item == NULL)
        {
            goto fail;
        }

        if (head == NULL)
        {
            current_item = head = new_item;
        }
        else
        {
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer))
        {
            goto fail;
        }
        buffer_skip_whitespace(input_buffer);
    }
    while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (cannot_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != ']')
    {
        goto fail;
    }

success:
    input_buffer->depth--;

    if (head != NULL) {
        head->prev = current_item;
    }

    item->type = cJSON_Array;
    item->child = head;

    input_buffer->offset++;

    return true;

fail:
    if (head != NULL)
    {
        cJSON_Delete(head);
    }

    return false;
}

static cJSON_bool print_array(const cJSON * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    size_t length = 0;
    cJSON *current_element = item->child;

    if (output_buffer == NULL)
    {
        return false;
    }

    output_pointer = ensure(output_buffer, 1);
    if (output_pointer == NULL)
    {
        return false;
    }

    *output_pointer = '[';
    output_buffer->offset++;
    output_buffer->depth++;

    while (current_element != NULL)
    {
        if (!print_value(current_element, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);
        if (current_element->next)
        {
            length = (size_t) (output_buffer->format ? 2 : 1);
            output_pointer = ensure(output_buffer, length + 1);
            if (output_pointer == NULL)
            {
                return false;
            }
            *output_pointer++ = ',';
            if(output_buffer->format)
            {
                *output_pointer++ = ' ';
            }
            *output_pointer = '\0';
            output_buffer->offset += length;
        }
        current_element = current_element->next;
    }

    output_pointer = ensure(output_buffer, 2);
    if (output_pointer == NULL)
    {
        return false;
    }
    *output_pointer++ = ']';
    *output_pointer = '\0';
    output_buffer->depth--;

    return true;
}

static cJSON_bool parse_object(cJSON * const item, parse_buffer * const input_buffer)
{
    cJSON *head = NULL;
    cJSON *current_item = NULL;

    if (input_buffer->depth >= CJSON_NESTING_LIMIT)
    {
        return false;
    }
    input_buffer->depth++;

    if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '{'))
    {
        goto fail;
    }

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '}'))
    {
        goto success;
    }

    if (cannot_access_at_index(input_buffer, 0))
    {
        input_buffer->offset--;
        goto fail;
    }

    input_buffer->offset--;
    do
    {
        cJSON *new_item = cJSON_New_Item(&(input_buffer->hooks));
        if (new_item == NULL)
        {
            goto fail;
        }

        if (head == NULL)
        {
            current_item = head = new_item;
        }
        else
        {
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        if (cannot_access_at_index(input_buffer, 1))
        {
            goto fail;
        }

        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_string(current_item, input_buffer))
        {
            goto fail;
        }
        buffer_skip_whitespace(input_buffer);

        current_item->string = current_item->valuestring;
        current_item->valuestring = NULL;

        if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != ':'))
        {
            goto fail;
        }

        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer))
        {
            goto fail;
        }
        buffer_skip_whitespace(input_buffer);
    }
    while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '}'))
    {
        goto fail;
    }

success:
    input_buffer->depth--;

    if (head != NULL) {
        head->prev = current_item;
    }

    item->type = cJSON_Object;
    item->child = head;

    input_buffer->offset++;
    return true;

fail:
    if (head != NULL)
    {
        cJSON_Delete(head);
    }

    return false;
}

static cJSON_bool print_object(const cJSON * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    size_t length = 0;
    cJSON *current_item = item->child;

    if (output_buffer == NULL)
    {
        return false;
    }

    length = (size_t) (output_buffer->format ? 2 : 1);
    output_pointer = ensure(output_buffer, length + 1);
    if (output_pointer == NULL)
    {
        return false;
    }

    *output_pointer++ = '{';
    output_buffer->depth++;
    if (output_buffer->format)
    {
        *output_pointer++ = '\n';
    }
    output_buffer->offset += length;

    while (current_item)
    {
        if (output_buffer->format)
        {
            size_t i;
            output_pointer = ensure(output_buffer, output_buffer->depth);
            if (output_pointer == NULL)
            {
                return false;
            }
            for (i = 0; i < output_buffer->depth; i++)
            {
                *output_pointer++ = '\t';
            }
            output_buffer->offset += output_buffer->depth;
        }

        if (!print_string_ptr((unsigned char*)current_item->string, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);

        length = (size_t) (output_buffer->format ? 2 : 1);
        output_pointer = ensure(output_buffer, length);
        if (output_pointer == NULL)
        {
            return false;
        }
        *output_pointer++ = ':';
        if (output_buffer->format)
        {
            *output_pointer++ = '\t';
        }
        output_buffer->offset += length;

        if (!print_value(current_item, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);

        length = ((size_t)(output_buffer->format ? 1 : 0) + (size_t)(current_item->next ? 1 : 0));
        output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == NULL)
        {
            return false;
        }
        if (current_item->next)
        {
            *output_pointer++ = ',';
        }

        if (output_buffer->format)
        {
            *output_pointer++ = '\n';
        }
        *output_pointer = '\0';
        output_buffer->offset += length;

        current_item = current_item->next;
    }

    output_pointer = ensure(output_buffer, output_buffer->format ? (output_buffer->depth + 1) : 2);
    if (output_pointer == NULL)
    {
        return false;
    }
    if (output_buffer->format)
    {
        size_t i;
        for (i = 0; i < (output_buffer->depth - 1); i++)
        {
            *output_pointer++ = '\t';
        }
    }
    *output_pointer++ = '}';
    *output_pointer = '\0';
    output_buffer->depth--;

    return true;
}

CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array)
{
    cJSON *child = NULL;
    size_t size = 0;

    if (array == NULL)
    {
        return 0;
    }

    child = array->child;

    while(child != NULL)
    {
        size++;
        child = child->next;
    }

    return (int)size;
}

static cJSON* get_array_item(const cJSON *array, size_t index)
{
    cJSON *current_child = NULL;

    if (array == NULL)
    {
        return NULL;
    }

    current_child = array->child;
    while ((current_child != NULL) && (index > 0))
    {
        index--;
        current_child = current_child->next;
    }

    return current_child;
}

CJSON_PUBLIC(cJSON *) cJSON_GetArrayItem(const cJSON *array, int index)
{
    if (index < 0)
    {
        return NULL;
    }

    return get_array_item(array, (size_t)index);
}

static cJSON *get_object_item(const cJSON * const object, const char * const name, const cJSON_bool case_sensitive)
{
    cJSON *current_element = NULL;

    if ((object == NULL) || (name == NULL))
    {
        return NULL;
    }

    current_element = object->child;
    if (case_sensitive)
    {
        while ((current_element != NULL) && (current_element->string != NULL) && (strcmp(name, current_element->string) != 0))
        {
            current_element = current_element->next;
        }
    }
    else
    {
        while ((current_element != NULL) && (case_insensitive_strcmp((const unsigned char*)name, (const unsigned char*)(current_element->string)) != 0))
        {
            current_element = current_element->next;
        }
    }

    if ((current_element == NULL) || (current_element->string == NULL)) {
        return NULL;
    }

    return current_element;
}

CJSON_PUBLIC(cJSON *) cJSON_GetObjectItem(const cJSON * const object, const char * const string)
{
    return get_object_item(object, string, false);
}

CJSON_PUBLIC(cJSON *) cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string)
{
    return get_object_item(object, string, true);
}

CJSON_PUBLIC(cJSON_bool) cJSON_HasObjectItem(const cJSON *object, const char *string)
{
    return cJSON_GetObjectItem(object, string) ? 1 : 0;
}

static void suffix_object(cJSON *prev, cJSON *item)
{
    prev->next = item;
    item->prev = prev;
}

static cJSON *create_reference(const cJSON *item, const internal_hooks * const hooks)
{
    cJSON *reference = NULL;
    if (item == NULL)
    {
        return NULL;
    }

    reference = cJSON_New_Item(hooks);
    if (reference == NULL)
    {
        return NULL;
    }

    memcpy(reference, item, sizeof(cJSON));
    reference->string = NULL;
    reference->type |= cJSON_IsReference;
    reference->next = reference->prev = NULL;
    return reference;
}

static cJSON_bool add_item_to_array(cJSON *array, cJSON *item)
{
    cJSON *child = NULL;

    if ((item == NULL) || (array == NULL) || (array == item))
    {
        return false;
    }

    child = array->child;

    if (child == NULL)
    {
        array->child = item;
        item->prev = item;
        item->next = NULL;
    }
    else
    {
        if (child->prev)
        {
            suffix_object(child->prev, item);
            array->child->prev = item;
        }
    }

    return true;
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
    return add_item_to_array(array, item);
}

#if defined(__clang__) || (defined(__GNUC__)  && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))))
    #pragma GCC diagnostic push
#endif
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
static void* cast_away_const(const void* string)
{
    return (void*)string;
}
#if defined(__clang__) || (defined(__GNUC__)  && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))))
    #pragma GCC diagnostic pop
#endif


static cJSON_bool add_item_to_object(cJSON * const object, const char * const string, cJSON * const item, const internal_hooks * const hooks, const cJSON_bool constant_key)
{
    char *new_key = NULL;
    int new_type = cJSON_Invalid;

    if ((object == NULL) || (string == NULL) || (item == NULL) || (object == item))
    {
        return false;
    }

    if (constant_key)
    {
        new_key = (char*)cast_away_const(string);
        new_type = item->type | cJSON_StringIsConst;
    }
    else
    {
        new_key = (char*)cJSON_strdup((const unsigned char*)string, hooks);
        if (new_key == NULL)
        {
            return false;
        }

        new_type = item->type & ~cJSON_StringIsConst;
    }

    if (!(item->type & cJSON_StringIsConst) && (item->string != NULL))
    {
        hooks->deallocate(item->string);
    }

    item->string = new_key;
    item->type = new_type;

    return add_item_to_array(object, item);
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
    return add_item_to_object(object, string, item, &global_hooks, false);
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item)
{
    return add_item_to_object(object, string, item, &global_hooks, true);
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item)
{
    if (array == NULL)
    {
        return false;
    }

    return add_item_to_array(array, create_reference(item, &global_hooks));
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item)
{
    if ((object == NULL) || (string == NULL))
    {
        return false;
    }

    return add_item_to_object(object, string, create_reference(item, &global_hooks), &global_hooks, false);
}

CJSON_PUBLIC(cJSON*) cJSON_AddNullToObject(cJSON * const object, const char * const name)
{
    cJSON *null = cJSON_CreateNull();
    if (add_item_to_object(object, name, null, &global_hooks, false))
    {
        return null;
    }

    cJSON_Delete(null);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddTrueToObject(cJSON * const object, const char * const name)
{
    cJSON *true_item = cJSON_CreateTrue();
    if (add_item_to_object(object, name, true_item, &global_hooks, false))
    {
        return true_item;
    }

    cJSON_Delete(true_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddFalseToObject(cJSON * const object, const char * const name)
{
    cJSON *false_item = cJSON_CreateFalse();
    if (add_item_to_object(object, name, false_item, &global_hooks, false))
    {
        return false_item;
    }

    cJSON_Delete(false_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddBoolToObject(cJSON * const object, const char * const name, const cJSON_bool boolean)
{
    cJSON *bool_item = cJSON_CreateBool(boolean);
    if (add_item_to_object(object, name, bool_item, &global_hooks, false))
    {
        return bool_item;
    }

    cJSON_Delete(bool_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddNumberToObject(cJSON * const object, const char * const name, const double number)
{
    cJSON *number_item = cJSON_CreateNumber(number);
    if (add_item_to_object(object, name, number_item, &global_hooks, false))
    {
        return number_item;
    }

    cJSON_Delete(number_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddStringToObject(cJSON * const object, const char * const name, const char * const string)
{
    cJSON *string_item = cJSON_CreateString(string);
    if (add_item_to_object(object, name, string_item, &global_hooks, false))
    {
        return string_item;
    }

    cJSON_Delete(string_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddRawToObject(cJSON * const object, const char * const name, const char * const raw)
{
    cJSON *raw_item = cJSON_CreateRaw(raw);
    if (add_item_to_object(object, name, raw_item, &global_hooks, false))
    {
        return raw_item;
    }

    cJSON_Delete(raw_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddObjectToObject(cJSON * const object, const char * const name)
{
    cJSON *object_item = cJSON_CreateObject();
    if (add_item_to_object(object, name, object_item, &global_hooks, false))
    {
        return object_item;
    }

    cJSON_Delete(object_item);
    return NULL;
}

CJSON_PUBLIC(cJSON*) cJSON_AddArrayToObject(cJSON * const object, const char * const name)
{
    cJSON *array = cJSON_CreateArray();
    if (add_item_to_object(object, name, array, &global_hooks, false))
    {
        return array;
    }

    cJSON_Delete(array);
    return NULL;
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemViaPointer(cJSON *parent, cJSON * const item)
{
    if ((parent == NULL) || (item == NULL) || (item != parent->child && item->prev == NULL))
    {
        return NULL;
    }

    if (item != parent->child)
    {
        item->prev->next = item->next;
    }
    if (item->next != NULL)
    {
        item->next->prev = item->prev;
    }

    if (item == parent->child)
    {
        parent->child = item->next;
    }
    else if (item->next == NULL)
    {
        parent->child->prev = item->prev;
    }

    item->prev = NULL;
    item->next = NULL;

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromArray(cJSON *array, int which)
{
    if (which < 0)
    {
        return NULL;
    }

    return cJSON_DetachItemViaPointer(array, get_array_item(array, (size_t)which));
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromArray(cJSON *array, int which)
{
    cJSON_Delete(cJSON_DetachItemFromArray(array, which));
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObject(cJSON *object, const char *string)
{
    cJSON *to_detach = cJSON_GetObjectItem(object, string);

    return cJSON_DetachItemViaPointer(object, to_detach);
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string)
{
    cJSON *to_detach = cJSON_GetObjectItemCaseSensitive(object, string);

    return cJSON_DetachItemViaPointer(object, to_detach);
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromObject(cJSON *object, const char *string)
{
    cJSON_Delete(cJSON_DetachItemFromObject(object, string));
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string)
{
    cJSON_Delete(cJSON_DetachItemFromObjectCaseSensitive(object, string));
}

CJSON_PUBLIC(cJSON_bool) cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem)
{
    cJSON *after_inserted = NULL;

    if (which < 0 || newitem == NULL)
    {
        return false;
    }

    after_inserted = get_array_item(array, (size_t)which);
    if (after_inserted == NULL)
    {
        return add_item_to_array(array, newitem);
    }

    if (after_inserted != array->child && after_inserted->prev == NULL) {
        return false;
    }

    newitem->next = after_inserted;
    newitem->prev = after_inserted->prev;
    after_inserted->prev = newitem;
    if (after_inserted == array->child)
    {
        array->child = newitem;
    }
    else
    {
        newitem->prev->next = newitem;
    }
    return true;
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemViaPointer(cJSON * const parent, cJSON * const item, cJSON * replacement)
{
    if ((parent == NULL) || (parent->child == NULL) || (replacement == NULL) || (item == NULL))
    {
        return false;
    }

    if (replacement == item)
    {
        return true;
    }

    replacement->next = item->next;
    replacement->prev = item->prev;

    if (replacement->next != NULL)
    {
        replacement->next->prev = replacement;
    }
    if (parent->child == item)
    {
        if (parent->child->prev == parent->child)
        {
            replacement->prev = replacement;
        }
        parent->child = replacement;
    }
    else
    {
        if (replacement->prev != NULL)
        {
            replacement->prev->next = replacement;
        }
        if (replacement->next == NULL)
        {
            parent->child->prev = replacement;
        }
    }

    item->next = NULL;
    item->prev = NULL;
    cJSON_Delete(item);

    return true;
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem)
{
    if (which < 0)
    {
        return false;
    }

    return cJSON_ReplaceItemViaPointer(array, get_array_item(array, (size_t)which), newitem);
}

static cJSON_bool replace_item_in_object(cJSON *object, const char *string, cJSON *replacement, cJSON_bool case_sensitive)
{
    if ((replacement == NULL) || (string == NULL))
    {
        return false;
    }

    if (!(replacement->type & cJSON_StringIsConst) && (replacement->string != NULL))
    {
        cJSON_free(replacement->string);
    }
    replacement->string = (char*)cJSON_strdup((const unsigned char*)string, &global_hooks);
    if (replacement->string == NULL)
    {
        return false;
    }

    replacement->type &= ~cJSON_StringIsConst;

    return cJSON_ReplaceItemViaPointer(object, get_object_item(object, string, case_sensitive), replacement);
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInObject(cJSON *object, const char *string, cJSON *newitem)
{
    return replace_item_in_object(object, string, newitem, false);
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem)
{
    return replace_item_in_object(object, string, newitem, true);
}

CJSON_PUBLIC(cJSON *) cJSON_CreateNull(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_NULL;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateTrue(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_True;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateFalse(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_False;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateBool(cJSON_bool boolean)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = boolean ? cJSON_True : cJSON_False;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateNumber(double num)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_Number;
        item->valuedouble = num;

        if (num >= INT_MAX)
        {
            item->valueint = INT_MAX;
        }
        else if (num <= (double)INT_MIN)
        {
            item->valueint = INT_MIN;
        }
        else
        {
            item->valueint = (int)num;
        }
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateString(const char *string)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_String;
        item->valuestring = (char*)cJSON_strdup((const unsigned char*)string, &global_hooks);
        if(!item->valuestring)
        {
            cJSON_Delete(item);
            return NULL;
        }
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateStringReference(const char *string)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item != NULL)
    {
        item->type = cJSON_String | cJSON_IsReference;
        item->valuestring = (char*)cast_away_const(string);
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateObjectReference(const cJSON *child)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item != NULL) {
        item->type = cJSON_Object | cJSON_IsReference;
        item->child = (cJSON*)cast_away_const(child);
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateArrayReference(const cJSON *child) {
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item != NULL) {
        item->type = cJSON_Array | cJSON_IsReference;
        item->child = (cJSON*)cast_away_const(child);
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateRaw(const char *raw)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type = cJSON_Raw;
        item->valuestring = (char*)cJSON_strdup((const unsigned char*)raw, &global_hooks);
        if(!item->valuestring)
        {
            cJSON_Delete(item);
            return NULL;
        }
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if(item)
    {
        item->type=cJSON_Array;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void)
{
    cJSON *item = cJSON_New_Item(&global_hooks);
    if (item)
    {
        item->type = cJSON_Object;
    }

    return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateIntArray(const int *numbers, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateNumber(numbers[i]);
        if (!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

    return a;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateFloatArray(const float *numbers, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateNumber((double)numbers[i]);
        if(!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

    return a;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateDoubleArray(const double *numbers, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (numbers == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateNumber(numbers[i]);
        if(!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

    return a;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateStringArray(const char *const *strings, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if ((count < 0) || (strings == NULL))
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for (i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateString(strings[i]);
        if(!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p,n);
        }
        p = n;
    }

    if (a && a->child) {
        a->child->prev = n;
    }

    return a;
}

cJSON * cJSON_Duplicate_rec(const cJSON *item, size_t depth, cJSON_bool recurse);

CJSON_PUBLIC(cJSON *) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse)
{
    return cJSON_Duplicate_rec(item, 0, recurse );
}

cJSON * cJSON_Duplicate_rec(const cJSON *item, size_t depth, cJSON_bool recurse)
{
    cJSON *newitem = NULL;
    cJSON *child = NULL;
    cJSON *next = NULL;
    cJSON *newchild = NULL;

    if (!item)
    {
        goto fail;
    }
    newitem = cJSON_New_Item(&global_hooks);
    if (!newitem)
    {
        goto fail;
    }
    newitem->type = item->type & (~cJSON_IsReference);
    newitem->valueint = item->valueint;
    newitem->valuedouble = item->valuedouble;
    if (item->valuestring)
    {
        newitem->valuestring = (char*)cJSON_strdup((unsigned char*)item->valuestring, &global_hooks);
        if (!newitem->valuestring)
        {
            goto fail;
        }
    }
    if (item->string)
    {
        newitem->string = (item->type&cJSON_StringIsConst) ? item->string : (char*)cJSON_strdup((unsigned char*)item->string, &global_hooks);
        if (!newitem->string)
        {
            goto fail;
        }
    }
    if (!recurse)
    {
        return newitem;
    }
    child = item->child;
    while (child != NULL)
    {
        if(depth >= CJSON_CIRCULAR_LIMIT) {
            goto fail;
        }
        newchild = cJSON_Duplicate_rec(child, depth + 1, true);
        if (!newchild)
        {
            goto fail;
        }
        if (next != NULL)
        {
            next->next = newchild;
            newchild->prev = next;
            next = newchild;
        }
        else
        {
            newitem->child = newchild;
            next = newchild;
        }
        child = child->next;
    }
    if (newitem && newitem->child)
    {
        newitem->child->prev = newchild;
    }

    return newitem;

fail:
    if (newitem != NULL)
    {
        cJSON_Delete(newitem);
    }

    return NULL;
}

static void skip_oneline_comment(char **input)
{
    *input += static_strlen("//");

    for (; (*input)[0] != '\0'; ++(*input))
    {
        if ((*input)[0] == '\n') {
            *input += static_strlen("\n");
            return;
        }
    }
}

static void skip_multiline_comment(char **input)
{
    *input += static_strlen("/*");

    for (; (*input)[0] != '\0'; ++(*input))
    {
        if (((*input)[0] == '*') && ((*input)[1] == '/'))
        {
            *input += static_strlen("*/");
            return;
        }
    }
}

static void minify_string(char **input, char **output) {
    (*output)[0] = (*input)[0];
    *input += static_strlen("\"");
    *output += static_strlen("\"");


    for (; (*input)[0] != '\0'; (void)++(*input), ++(*output)) {
        (*output)[0] = (*input)[0];

        if ((*input)[0] == '\"') {
            (*output)[0] = '\"';
            *input += static_strlen("\"");
            *output += static_strlen("\"");
            return;
        } else if (((*input)[0] == '\\') && ((*input)[1] == '\"')) {
            (*output)[1] = (*input)[1];
            *input += static_strlen("\"");
            *output += static_strlen("\"");
        }
    }
}

CJSON_PUBLIC(void) cJSON_Minify(char *json)
{
    char *into = json;

    if (json == NULL)
    {
        return;
    }

    while (json[0] != '\0')
    {
        switch (json[0])
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                json++;
                break;

            case '/':
                if (json[1] == '/')
                {
                    skip_oneline_comment(&json);
                }
                else if (json[1] == '*')
                {
                    skip_multiline_comment(&json);
                } else {
                    json++;
                }
                break;

            case '\"':
                minify_string(&json, (char**)&into);
                break;

            default:
                into[0] = json[0];
                json++;
                into++;
        }
    }

    *into = '\0';
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsInvalid(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Invalid;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsFalse(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_False;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsTrue(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xff) == cJSON_True;
}


CJSON_PUBLIC(cJSON_bool) cJSON_IsBool(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & (cJSON_True | cJSON_False)) != 0;
}
CJSON_PUBLIC(cJSON_bool) cJSON_IsNull(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_NULL;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsNumber(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Number;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsString(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_String;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsArray(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Array;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsObject(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Object;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsRaw(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Raw;
}

CJSON_PUBLIC(cJSON_bool) cJSON_Compare(const cJSON * const a, const cJSON * const b, const cJSON_bool case_sensitive)
{
    if ((a == NULL) || (b == NULL) || ((a->type & 0xFF) != (b->type & 0xFF)))
    {
        return false;
    }

    switch (a->type & 0xFF)
    {
        case cJSON_False:
        case cJSON_True:
        case cJSON_NULL:
        case cJSON_Number:
        case cJSON_String:
        case cJSON_Raw:
        case cJSON_Array:
        case cJSON_Object:
            break;

        default:
            return false;
    }

    if (a == b)
    {
        return true;
    }

    switch (a->type & 0xFF)
    {
        case cJSON_False:
        case cJSON_True:
        case cJSON_NULL:
            return true;

        case cJSON_Number:
            if (compare_double(a->valuedouble, b->valuedouble))
            {
                return true;
            }
            return false;

        case cJSON_String:
        case cJSON_Raw:
            if ((a->valuestring == NULL) || (b->valuestring == NULL))
            {
                return false;
            }
            if (strcmp(a->valuestring, b->valuestring) == 0)
            {
                return true;
            }

            return false;

        case cJSON_Array:
        {
            cJSON *a_element = a->child;
            cJSON *b_element = b->child;

            for (; (a_element != NULL) && (b_element != NULL);)
            {
                if (!cJSON_Compare(a_element, b_element, case_sensitive))
                {
                    return false;
                }

                a_element = a_element->next;
                b_element = b_element->next;
            }

            if (a_element != b_element) {
                return false;
            }

            return true;
        }

        case cJSON_Object:
        {
            cJSON *a_element = NULL;
            cJSON *b_element = NULL;
            cJSON_ArrayForEach(a_element, a)
            {
                b_element = get_object_item(b, a_element->string, case_sensitive);
                if (b_element == NULL)
                {
                    return false;
                }

                if (!cJSON_Compare(a_element, b_element, case_sensitive))
                {
                    return false;
                }
            }

            cJSON_ArrayForEach(b_element, b)
            {
                a_element = get_object_item(a, b_element->string, case_sensitive);
                if (a_element == NULL)
                {
                    return false;
                }

                if (!cJSON_Compare(b_element, a_element, case_sensitive))
                {
                    return false;
                }
            }

            return true;
        }

        default:
            return false;
    }
}

CJSON_PUBLIC(void *) cJSON_malloc(size_t size)
{
    return global_hooks.allocate(size);
}

CJSON_PUBLIC(void) cJSON_free(void *object)
{
    global_hooks.deallocate(object);
    object = NULL;
}
