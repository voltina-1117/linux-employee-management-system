#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include <ctype.h>

/**
 * 数据库连接配置宏定义
 * DB_HOST: 数据库服务器地址，通常为localhost表示本地数据库
 * DB_USER: 数据库用户名，用于连接认证
 * DB_PASS: 数据库密码，用于连接认证  
 * DB_NAME: 要连接的数据库名称
 */
#define DB_HOST "localhost"
#define DB_USER "company_user"
#define DB_PASS "password123"
#define DB_NAME "company_db"

/**
 * 用户角色枚举定义
 * ROLE_BOSS:    老板角色，拥有最高权限
 * ROLE_MANAGER: 主管角色，拥有部门管理权限
 * ROLE_EMPLOYEE:员工角色，只有查看自身信息权限
 */
typedef enum {
    ROLE_BOSS,      // 老板 - 可以管理所有员工和查看所有信息
    ROLE_MANAGER,   // 主管 - 可以管理本部门员工
    ROLE_EMPLOYEE   // 员工 - 只能查看自己的信息
} UserRole;

/**
 * 当前登录用户信息结构体
 * username:    登录用户名
 * role:        用户角色（老板/主管/员工）
 * employee_id: 关联的员工工号
 * department:  所属部门（主管和员工使用）
 */
typedef struct {
    char username[50];      // 用户名，最大长度50字符
    UserRole role;          // 用户角色枚举值
    char employee_id[50];   // 员工工号，用于关联员工表
    char department[100];   // 所属部门名称
} CurrentUser;

// 全局变量 - 当前登录用户信息，在整个程序运行期间保持
CurrentUser current_user;

// 函数声明
MYSQL* connect_to_database();
void close_database(MYSQL *conn);
int login_system();
int initialize_database();
int add_employee();
int update_employee();
int delete_employee();
int view_employees_by_role();
int view_employees_by_department_or_id();
int view_monthly_salary();
void display_menu_by_role();
void clear_input_buffer();
void to_uppercase(char *str);


/*
// 连接MySQL数据库，返回连接句柄（失败返回NULL）
MYSQL* connect_to_database();
// 关闭数据库连接，释放资源
void close_database(MYSQL *conn);
// 系统登录验证，返回0失败/1成功（可扩展角色返回值）
int login_system();
// 初始化数据库表结构，返回0失败/1成功
int initialize_database();
// 添加新员工信息，返回0失败/1成功
int add_employee();
// 修改员工信息，返回0失败/1成功
int update_employee();
// 删除员工信息，返回0失败/1成功
int delete_employee();
// 按职位（角色）查询员工列表，返回0无数据/1有数据
int view_employees_by_role();
// 按部门/员工ID查询员工信息，返回0无数据/1有数据
int view_employees_by_department_or_id();
// 查看员工月薪信息，返回0无数据/1有数据
int view_monthly_salary();
// 按登录角色显示对应功能菜单
void display_menu_by_role();
// 清空输入缓冲区，解决换行符残留问题
void clear_input_buffer();
// 将字符串转换为大写，统一输入格式
void to_uppercase(char *str);*/
/**
 * @brief 清除输入缓冲区中的剩余字符
 * 
 * 当使用scanf等函数读取输入后，输入缓冲区中可能还有换行符或其他字符，
 * 这些残留字符会影响后续的输入操作。此函数会清空缓冲区直到遇到换行符或文件结束符。
 * 
 * 使用场景：在每次使用scanf后调用，确保输入缓冲区干净
 */
void clear_input_buffer() {
    int c;
    // 循环读取并丢弃缓冲区中的字符，直到遇到换行符或文件结束符
    while ((c = getchar()) != '\n' && c != EOF);
}

/**
 * @brief 将字符串转换为大写形式
 * 
 * 此函数用于将用户输入的工号等字符串统一转换为大写，避免大小写不一致导致的匹配问题。
 * 例如：用户输入"emp001"会被转换为"EMP001"
 * 
 * @param str 要转换的字符串指针，转换会直接修改原字符串
 */
void to_uppercase(char *str) {
    // 遍历字符串中的每个字符，使用toupper函数转换为大写
    for (int i = 0; str[i]; i++) {
        str[i] = toupper(str[i]);
    }
}

/**
 * @brief 建立到MySQL数据库的连接
 * 
 * 此函数初始化MySQL连接对象并尝试连接到指定的数据库。
 * 如果连接成功，返回连接句柄；如果失败，返回NULL并打印错误信息。
 * 
 * @return MYSQL* 成功返回数据库连接指针，失败返回NULL
 * 
 * 连接步骤：
 * 1. mysql_init() - 初始化连接对象
 * 2. mysql_real_connect() - 建立实际连接
 * 3. 错误处理 - 如果任何步骤失败，清理资源并返回NULL
 */
MYSQL* connect_to_database() {
    // 初始化MySQL连接对象
    MYSQL *conn = mysql_init(NULL);
    
    // 检查初始化是否成功
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() 失败: 无法分配内存创建连接对象\n");
        return NULL;
    }
    
    /**
     * 建立实际的数据库连接
     * 参数说明：
     * conn: 已初始化的连接对象
     * DB_HOST: 数据库服务器地址
     * DB_USER: 数据库用户名
     * DB_PASS: 数据库密码
     * DB_NAME: 要连接的数据库名
     * 0: 使用默认MySQL端口（3306）
     * NULL: 使用默认Unix socket
     * 0: 无特殊客户端标志
     */
    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) {
        // 连接失败，打印错误信息并清理资源
        fprintf(stderr, "mysql_real_connect() 失败: %s\n", mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }
    
    // 连接成功，返回连接对象
    return conn;
}

/**
 * @brief 关闭数据库连接
 * 
 * 安全地关闭数据库连接并释放相关资源。
 * 在每次数据库操作完成后应该调用此函数，避免连接泄露。
 * 
 * @param conn 要关闭的数据库连接指针
 */
void close_database(MYSQL *conn) {
    if (conn != NULL) {
        mysql_close(conn);
    }
}

/**
 * @brief 用户登录验证系统
 * 
 * 此函数处理用户登录流程，包括：
 * 1. 获取用户输入的用户名和密码
 * 2. 查询数据库验证用户身份
 * 3. 设置当前用户的角色和权限信息
 * 4. 初始化全局变量current_user
 * 
 * @return int 登录成功返回1，失败返回0
 * 
 * 登录流程：
 * 1. 用户输入用户名和密码
 * 2. 构建SQL查询语句检查用户是否存在
 * 3. 验证密码是否正确
 * 4. 设置用户角色和关联信息
 */
int login_system() {
    MYSQL *conn;            // 数据库连接对象
    MYSQL_RES *result;      // 查询结果集
    MYSQL_ROW row;          // 结果集中的一行数据
    char username[50];      // 存储用户输入的用户名
    char password[50];      // 存储用户输入的密码
    char query[512];        // SQL查询语句缓冲区
    
    printf("\n=== 员工管理系统登录 ===\n");
    
    // 获取用户名输入
    printf("用户名: ");
    // 使用scanf读取用户名，限制长度为49字符防止缓冲区溢出
    if (scanf("%49s", username) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    // 获取密码输入
    printf("密码: ");
    if (scanf("%49s", password) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    // 清理输入缓冲区，防止残留字符影响后续输入
    clear_input_buffer();
    
    // 连接到数据库
    conn = connect_to_database();
    if (conn == NULL) {
        return 0;
    }
    
    /**
     * 构建SQL查询语句，查询用户信息
     * 查询字段：密码、角色、员工工号、部门
     * 使用LEFT JOIN关联员工表，因为老板可能没有对应的员工记录
     */
    sprintf(query, "SELECT u.password, u.role, u.employee_id, e.department FROM users u LEFT JOIN employees e ON u.employee_id = e.employee_id WHERE u.username = '%s'", username);
    


    // 执行SQL查询
    if (mysql_query(conn, query)) {
        fprintf(stderr, "查询失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    // 获取查询结果集
    result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "存储结果失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    // 从结果集中获取第一行数据
    row = mysql_fetch_row(result);
    if (row == NULL) {
        // 没有找到用户记录
        printf("用户名或密码错误！\n");
        mysql_free_result(result);
        close_database(conn);
        return 0;
    }
    
    // 验证密码是否正确
    if (strcmp(row[0], password) != 0) {
        printf("用户名或密码错误！\n");
        mysql_free_result(result);
        close_database(conn);
        return 0;
    }
    
    // 设置当前用户信息到全局变量
    strcpy(current_user.username, username);
    // row[2]是employee_id，可能为NULL（如老板账号），使用条件表达式处理
    strcpy(current_user.employee_id, row[2] ? row[2] : "");
    // row[3]是department，可能为NULL，使用条件表达式处理
    strcpy(current_user.department, row[3] ? row[3] : "");
    
    // 根据数据库中的角色字符串设置用户角色枚举值
    if (strcmp(row[1], "老板") == 0) {
        current_user.role = ROLE_BOSS;
    } else if (strcmp(row[1], "主管") == 0) {
        current_user.role = ROLE_MANAGER;
    } else {
        current_user.role = ROLE_EMPLOYEE;
    }
    
    // 登录成功，显示欢迎信息
    printf("\n登录成功！欢迎 %s [%s]\n", username, row[1]);
    
    // 释放结果集和关闭数据库连接
    mysql_free_result(result);
    close_database(conn);
    return 1;
}

/**
 * @brief 初始化数据库表结构和默认数据
 * 
 * 此函数用于创建数据库表并插入初始测试数据。
 * 注意：这会删除现有的表和数据，用于系统首次部署或重置。
 * 
 * @return int 初始化成功返回1，失败返回0
 * 
 * 初始化步骤：
 * 1. 确认用户操作（因为会删除现有数据）
 * 2. 按依赖顺序删除现有表
 * 3. 创建新表（employees → users → salary_records）
 * 4. 插入默认数据
 * 5. 创建索引优化查询性能
 */
int initialize_database() {
    MYSQL *conn;        // 数据库连接对象
    char confirm;       // 用户确认字符
    
    printf("\n=== 数据库初始化 ===\n");
    printf("警告：这将重置所有数据！确定要继续吗？(y/n): ");
    
    // 获取用户确认，防止误操作
    if (scanf(" %c", &confirm) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    clear_input_buffer();
    
    // 检查用户确认
    if (confirm != 'y' && confirm != 'Y') {
        printf("初始化已取消。\n");
        return 0;
    }
    
    // 连接到数据库
    conn = connect_to_database();
    if (conn == NULL) {
        return 0;
    }
    
    // 按依赖关系顺序删除现有表
    mysql_query(conn, "DROP TABLE IF EXISTS salary_records");
    mysql_query(conn, "DROP TABLE IF EXISTS users");
    mysql_query(conn, "DROP TABLE IF EXISTS employees");
    
    // 创建员工表
    if (mysql_query(conn, "CREATE TABLE employees (id INT AUTO_INCREMENT PRIMARY KEY, employee_id VARCHAR(50) UNIQUE NOT NULL, name VARCHAR(100) NOT NULL, gender ENUM('男', '女') NOT NULL, age INT NOT NULL, salary DECIMAL(10,2) NOT NULL, department VARCHAR(100) NOT NULL, position VARCHAR(100) NOT NULL, hire_date DATE NOT NULL, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)")) {
        fprintf(stderr, "创建员工表失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    } else {
        printf("员工表创建成功\n");
    }
    
    // 插入默认员工数据
    if (mysql_query(conn, "INSERT INTO employees (employee_id, name, gender, age, salary, department, position, hire_date) VALUES ('EMP001', '张三', '男', 28, 8000.00, '技术部', '软件工程师', '2022-03-15'), ('EMP002', '李四', '女', 32, 12000.00, '技术部', '技术主管', '2020-06-01'), ('EMP003', '王五', '男', 45, 20000.00, '管理部', '总经理', '2018-01-10'), ('EMP004', '赵六', '女', 26, 6000.00, '销售部', '销售专员', '2023-02-20'), ('EMP005', '钱七', '男', 35, 15000.00, '销售部', '销售主管', '2019-08-15')")) {
        fprintf(stderr, "插入员工数据失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    } else {
        printf("员工数据插入成功\n");
    }
    
    // 创建用户表
    if (mysql_query(conn, "CREATE TABLE users (id INT AUTO_INCREMENT PRIMARY KEY, username VARCHAR(50) UNIQUE NOT NULL, password VARCHAR(255) NOT NULL, role ENUM('老板', '主管', '员工') NOT NULL, employee_id VARCHAR(50), created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, FOREIGN KEY (employee_id) REFERENCES employees(employee_id) ON DELETE SET NULL)")) {
        fprintf(stderr, "创建用户表失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    } else {
        printf("用户表创建成功\n");
    }
    
    // 插入默认用户数据
    if (mysql_query(conn, "INSERT INTO users (username, password, role, employee_id) VALUES ('boss', '123456', '老板', 'EMP003'), ('manager1', '123456', '主管', 'EMP002'), ('manager2', '123456', '主管', 'EMP005'), ('staff1', '123456', '员工', 'EMP001'), ('staff2', '123456', '员工', 'EMP004')")) {
        fprintf(stderr, "插入用户数据失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    } else {
        printf("用户数据插入成功\n");
    }
    
    // 创建工资记录表
    if (mysql_query(conn, "CREATE TABLE salary_records (id INT AUTO_INCREMENT PRIMARY KEY, employee_id VARCHAR(50) NOT NULL, salary_month DATE NOT NULL, base_salary DECIMAL(10,2) NOT NULL, bonus DECIMAL(10,2) DEFAULT 0.00, deduction DECIMAL(10,2) DEFAULT 0.00, actual_salary DECIMAL(10,2) NOT NULL, created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, FOREIGN KEY (employee_id) REFERENCES employees(employee_id) ON DELETE CASCADE)")) {
        fprintf(stderr, "创建工资记录表失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    } else {
        printf("工资记录表创建成功\n");
    }
    
    // 插入工资记录数据（3个月的示例数据）
    if (mysql_query(conn, "INSERT INTO salary_records (employee_id, salary_month, base_salary, bonus, deduction, actual_salary) VALUES ('EMP001', '2024-01-01', 8000.00, 500.00, 200.00, 8300.00), ('EMP002', '2024-01-01', 12000.00, 1000.00, 300.00, 12700.00), ('EMP003', '2024-01-01', 20000.00, 3000.00, 500.00, 22500.00), ('EMP004', '2024-01-01', 6000.00, 800.00, 100.00, 6700.00), ('EMP005', '2024-01-01', 15000.00, 2000.00, 400.00, 16600.00), ('EMP001', '2024-02-01', 8000.00, 600.00, 150.00, 8450.00), ('EMP002', '2024-02-01', 12000.00, 1200.00, 250.00, 12950.00), ('EMP003', '2024-02-01', 20000.00, 3500.00, 450.00, 23050.00), ('EMP004', '2024-02-01', 6000.00, 1000.00, 80.00, 6920.00), ('EMP005', '2024-02-01', 15000.00, 2500.00, 350.00, 17150.00), ('EMP001', '2024-03-01', 8000.00, 700.00, 180.00, 8520.00), ('EMP002', '2024-03-01', 12000.00, 1500.00, 280.00, 13220.00), ('EMP003', '2024-03-01', 20000.00, 4000.00, 520.00, 23480.00), ('EMP004', '2024-03-01', 6000.00, 1200.00, 90.00, 7110.00), ('EMP005', '2024-03-01', 15000.00, 3000.00, 380.00, 17620.00)")) {
        fprintf(stderr, "插入工资记录失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    } else {
        printf("工资记录插入成功\n");
    }
    
    // 创建索引以提高查询性能
    mysql_query(conn, "CREATE INDEX idx_employee_department ON employees(department)");
    mysql_query(conn, "CREATE INDEX idx_salary_month ON salary_records(salary_month)");
    mysql_query(conn, "CREATE INDEX idx_employee_salary ON salary_records(employee_id, salary_month)");
    
    // 关闭数据库连接
    close_database(conn);
    
    printf("数据库初始化完成！\n");
    return 1;
}

/**
 * @brief 添加新员工信息（仅老板权限）
 * 
 * 此函数允许老板角色添加新的员工记录到数据库。
 * 会收集员工的所有基本信息并插入到employees表中。
 * 
 * @return int 添加成功返回1，失败返回0
 * 
 * 输入参数说明：
 * - 工号: 员工唯一标识，如"EMP006"，会自动转换为大写
 * - 姓名: 员工姓名，如"张三"
 * - 性别: 必须是"男"或"女"
 * - 年龄: 整数，如28
 * - 工资: 浮点数，如8000.00
 * - 部门: 部门名称，如"技术部"
 * - 职位: 职位名称，如"软件工程师"
 * - 入职日期: 格式YYYY-MM-DD，如"2024-01-15"
 */
int add_employee() {
    // 权限检查：只有老板可以添加员工
    if (current_user.role != ROLE_BOSS) {
        printf("权限不足！只有老板可以添加员工。\n");
        return 0;
    }
    
    MYSQL *conn;                // 数据库连接对象
    char employee_id[50];       // 员工工号
    char name[100];             // 员工姓名
    char gender[10];            // 员工性别
    int age;                    // 员工年龄
    double salary;              // 员工工资
    char department[100];       // 所属部门
    char position[100];         // 职位
    char hire_date[20];         // 入职日期
    char query[1024];           // SQL语句缓冲区
    
    printf("\n=== 添加新员工 ===\n");
    
    // 获取员工工号并转换为大写
    printf("工号: ");
    if (scanf("%49s", employee_id) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    to_uppercase(employee_id);
    
    // 获取员工姓名
    printf("姓名: ");
    if (scanf("%99s", name) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    // 获取员工性别
    printf("性别(男/女): ");
    if (scanf("%9s", gender) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    // 获取员工年龄
    printf("年龄: ");
    if (scanf("%d", &age) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    // 获取员工工资
    printf("工资: ");
    if (scanf("%lf", &salary) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    // 获取所属部门
    printf("部门: ");
    if (scanf("%99s", department) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    // 获取职位
    printf("职位: ");
    if (scanf("%99s", position) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    // 获取入职日期
    printf("入职日期(YYYY-MM-DD): ");
    if (scanf("%19s", hire_date) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    // 清理输入缓冲区
    clear_input_buffer();
    
    // 连接到数据库
    conn = connect_to_database();
    if (conn == NULL) {
        return 0;
    }
    
    // 构建INSERT SQL语句，将员工信息插入数据库
    sprintf(query, "INSERT INTO employees (employee_id, name, gender, age, salary, department, position, hire_date) VALUES ('%s', '%s', '%s', %d, %.2f, '%s', '%s', '%s')", 
            employee_id, name, gender, age, salary, department, position, hire_date);
    
    // 执行插入操作
    if (mysql_query(conn, query)) {
        fprintf(stderr, "执行插入失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    // 插入成功，显示成功信息
    printf("员工 %s (工号: %s) 添加成功！\n", name, employee_id);
    
    // 关闭数据库连接
    close_database(conn);
    return 1;
}

/**
 * @brief 修改员工信息（老板和主管权限）
 * 
 * 此函数允许老板和主管修改员工信息。
 * 老板可以修改所有员工，主管只能修改本部门员工。
 * 
 * @return int 修改成功返回1，失败返回0
 * 
 * 操作流程：
 * 1. 显示当前员工列表
 * 2. 输入要修改的员工工号
 * 3. 权限检查（主管只能修改本部门）
 * 4. 输入新的员工信息
 * 5. 更新数据库
 */
int update_employee() {
    // 权限检查：员工角色没有修改权限
    if (current_user.role == ROLE_EMPLOYEE) {
        printf("权限不足！只有老板和主管可以修改员工信息。\n");
        return 0;
    }
    
    MYSQL *conn;                // 数据库连接对象
    char employee_id[50];       // 要修改的员工工号
    char name[100];             // 新的姓名
    char gender[10];            // 新的性别
    int age;                    // 新的年龄
    double salary;              // 新的工资
    char department[100];       // 新的部门
    char position[100];         // 新的职位
    char query[1024];           // SQL语句缓冲区
    
    printf("\n=== 修改员工信息 ===\n");
    
    // 先显示员工列表，方便用户选择
    view_employees_by_role();
    
    // 获取要修改的员工工号
    printf("\n请输入要修改的员工工号: ");
    if (scanf("%49s", employee_id) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    to_uppercase(employee_id);      // 转换为大写确保匹配
    clear_input_buffer();           // 清理缓冲区
    
    // 权限检查：主管只能修改本部门员工
    if (current_user.role == ROLE_MANAGER) {
        MYSQL *check_conn = connect_to_database();
        if (check_conn != NULL) {
            char check_query[512];
            // 查询该员工所属部门
            sprintf(check_query, "SELECT department FROM employees WHERE employee_id = '%s'", employee_id);
            
            if (mysql_query(check_conn, check_query) == 0) {
                MYSQL_RES *result = mysql_store_result(check_conn);
                if (result != NULL) {
                    MYSQL_ROW row = mysql_fetch_row(result);
                    // 检查员工部门是否与主管部门匹配
                    if (row != NULL && strcmp(row[0], current_user.department) != 0) {
                        printf("权限不足！您只能修改本部门(%s)的员工。\n", current_user.department);
                        mysql_free_result(result);
                        close_database(check_conn);
                        return 0;
                    }
                    mysql_free_result(result);
                }
            }
            close_database(check_conn);
        }
    }
    
    // 获取新的员工信息
    printf("新的姓名: ");
    if (scanf("%99s", name) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    printf("新的性别(男/女): ");
    if (scanf("%9s", gender) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    printf("新的年龄: ");
    if (scanf("%d", &age) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    printf("新的工资: ");
    if (scanf("%lf", &salary) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    printf("新的部门: ");
    if (scanf("%99s", department) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    printf("新的职位: ");
    if (scanf("%99s", position) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    
    clear_input_buffer();
    
    // 连接到数据库
    conn = connect_to_database();
    if (conn == NULL) {
        return 0;
    }
    
    // 构建UPDATE SQL语句
    sprintf(query, "UPDATE employees SET name = '%s', gender = '%s', age = %d, salary = %.2f, department = '%s', position = '%s' WHERE employee_id = '%s'", 
            name, gender, age, salary, department, position, employee_id);
    
    // 执行更新操作
    if (mysql_query(conn, query)) {
        fprintf(stderr, "执行更新失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    printf("员工信息更新成功！\n");
    
    close_database(conn);
    return 1;
}

/**
 * @brief 删除员工信息（仅老板权限）
 * 
 * 此函数允许老板删除员工记录。
 * 包含确认机制防止误删，并会自动删除相关的用户和工资记录（外键约束）。
 * 
 * @return int 删除成功返回1，失败返回0
 * 
 * 删除流程：
 * 1. 显示员工列表
 * 2. 输入要删除的员工工号
 * 3. 确认删除操作
 * 4. 执行删除
 */
int delete_employee() {
    // 权限检查：只有老板可以删除员工
    if (current_user.role != ROLE_BOSS) {
        printf("权限不足！只有老板可以删除员工。\n");
        return 0;
    }
    
    MYSQL *conn;            // 数据库连接对象
    char employee_id[50];   // 要删除的员工工号
    char confirm;           // 确认字符
    char query[512];        // SQL语句缓冲区
    
    printf("\n=== 删除员工信息 ===\n");
    
    // 先显示所有员工，方便用户选择
    view_employees_by_role();
    
    // 获取要删除的员工工号
    printf("\n请输入要删除的员工工号: ");
    if (scanf("%49s", employee_id) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    to_uppercase(employee_id);  // 转换为大写
    
    // 确认删除操作，防止误删
    printf("确定要删除工号为 %s 的员工吗？此操作不可撤销！(y/n): ", employee_id);
    if (scanf(" %c", &confirm) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    clear_input_buffer();
    
    // 检查用户确认
    if (confirm != 'y' && confirm != 'Y') {
        printf("删除操作已取消。\n");
        return 0;
    }
    
    // 连接到数据库
    conn = connect_to_database();
    if (conn == NULL) {
        return 0;
    }
    
    // 构建DELETE SQL语句
    sprintf(query, "DELETE FROM employees WHERE employee_id = '%s'", employee_id);
    
    // 执行删除操作
    if (mysql_query(conn, query)) {
        fprintf(stderr, "执行删除失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    printf("员工工号 %s 删除成功！\n", employee_id);
    
    close_database(conn);
    return 1;
}

/**
 * @brief 根据用户角色查看员工信息
 * 
 * 此函数根据当前登录用户的角色显示不同的员工信息：
 * - 老板：查看所有员工的所有信息
 * - 主管：查看本部门员工的所有信息  
 * - 员工：只能查看自己的信息
 * 
 * @return int 查询成功返回1，失败返回0
 * 
 * 显示信息包括：
 * 工号、姓名、性别、年龄、工资、部门、职位、入职日期
 */
int view_employees_by_role() {
    MYSQL *conn;            // 数据库连接对象
    MYSQL_RES *result;      // 查询结果集
    MYSQL_ROW row;          // 结果集中的一行数据
    char query[1024];       // SQL查询语句缓冲区
    
    printf("\n=== 员工信息列表 ===\n");
    
    // 连接到数据库
    conn = connect_to_database();
    if (conn == NULL) {
        return 0;
    }
    
    // 根据用户角色构建不同的查询语句
    switch (current_user.role) {
        case ROLE_BOSS:
            // 老板可以查看所有员工的所有信息，按部门和工号排序
            strcpy(query, "SELECT employee_id, name, gender, age, salary, department, position, hire_date FROM employees ORDER BY department, employee_id");
            break;
            
        case ROLE_MANAGER:
            // 主管只能查看本部门员工的所有信息，按工号排序
            sprintf(query, "SELECT employee_id, name, gender, age, salary, department, position, hire_date FROM employees WHERE department = '%s' ORDER BY employee_id", current_user.department);
            break;
            
        case ROLE_EMPLOYEE:
            // 员工只能查看自己的信息
            sprintf(query, "SELECT employee_id, name, gender, age, salary, department, position, hire_date FROM employees WHERE employee_id = '%s'", current_user.employee_id);
            break;
    }
    
    // 执行查询
    if (mysql_query(conn, query)) {
        fprintf(stderr, "查询失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    // 获取查询结果
    result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "存储结果失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    // 显示表头
    printf("%-10s %-15s %-8s %-6s %-10s %-15s %-15s %-12s\n", 
           "工号", "姓名", "性别", "年龄", "工资", "部门", "职位", "入职日期");
    printf("--------------------------------------------------------------------------------------------\n");
    
    // 遍历结果集，显示每一行数据
    int count = 0;
    while ((row = mysql_fetch_row(result)) != NULL) {
        // 使用条件表达式处理可能的NULL值
        printf("%-10s %-15s %-8s %-6s %-10s %-15s %-15s %-12s\n", 
               row[0] ? row[0] : "NULL",      // 工号
               row[1] ? row[1] : "NULL",      // 姓名
               row[2] ? row[2] : "NULL",      // 性别
               row[3] ? row[3] : "NULL",      // 年龄
               row[4] ? row[4] : "NULL",      // 工资
               row[5] ? row[5] : "NULL",      // 部门
               row[6] ? row[6] : "NULL",      // 职位
               row[7] ? row[7] : "NULL");     // 入职日期
        count++;
    }
    
    // 显示记录总数
    printf("总计: %d 名员工\n", count);
    
    // 释放结果集和关闭连接
    mysql_free_result(result);
    close_database(conn);
    return 1;
}

/**
 * @brief 按部门或工号查询员工信息（只能查看工号和姓名）
 * 
 * 此函数提供两种查询方式：
 * 1. 按部门查询：显示指定部门的所有员工的工号和姓名
 * 2. 按工号查询：显示指定工号的员工的工号和姓名
 * 
 * 所有角色都可以使用此功能，但只能看到工号和姓名。
 * 
 * @return int 查询成功返回1，失败返回0
 */
int view_employees_by_department_or_id() {
    MYSQL *conn;                // 数据库连接对象
    MYSQL_RES *result;          // 查询结果集
    MYSQL_ROW row;              // 结果集中的一行数据
    char query[512];            // SQL查询语句缓冲区
    int choice;                 // 用户选择的查询方式
    char search_value[100];     // 搜索关键词（部门名称或工号）
    
    printf("\n=== 按条件查询员工 ===\n");
    printf("1. 按部门查询\n");
    printf("2. 按工号查询\n");
    printf("请选择查询方式: ");
    
    // 获取用户选择的查询方式
    if (scanf("%d", &choice) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    clear_input_buffer();
    
    // 根据用户选择构建不同的查询
    if (choice == 1) {
        // 按部门查询
        printf("请输入部门名称: ");
        if (scanf("%99s", search_value) != 1) {
            printf("输入错误！\n");
            clear_input_buffer();
            return 0;
        }
        
        // 构建按部门查询的SQL语句
        sprintf(query, "SELECT employee_id, name FROM employees WHERE department = '%s' ORDER BY employee_id", search_value);
    } else if (choice == 2) {
        // 按工号查询
        printf("请输入工号: ");
        if (scanf("%99s", search_value) != 1) {
            printf("输入错误！\n");
            clear_input_buffer();
            return 0;
        }
        to_uppercase(search_value);  // 转换为大写确保匹配
        
        // 构建按工号查询的SQL语句
        sprintf(query, "SELECT employee_id, name FROM employees WHERE employee_id = '%s'", search_value);
    } else {
        // 无效选择
        printf("无效的选择！\n");
        return 0;
    }
    
    clear_input_buffer();
    
    // 连接到数据库
    conn = connect_to_database();
    if (conn == NULL) {
        return 0;
    }
    
    // 执行查询
    if (mysql_query(conn, query)) {
        fprintf(stderr, "查询失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    // 获取查询结果
    result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "存储结果失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    // 显示查询结果表头
    printf("\n%-10s %-15s\n", "工号", "姓名");
    printf("--------------------\n");
    
    // 遍历结果集，显示查询结果
    int count = 0;
    while ((row = mysql_fetch_row(result)) != NULL) {
        printf("%-10s %-15s\n", 
               row[0] ? row[0] : "NULL",  // 工号
               row[1] ? row[1] : "NULL"); // 姓名
        count++;
    }
    
    // 显示找到的记录数
    printf("找到 %d 名员工\n", count);
    
    // 释放结果集和关闭连接
    mysql_free_result(result);
    close_database(conn);
    return 1;
}

/**
 * @brief 按月查看员工工资信息（仅老板权限）
 * 
 * 此函数允许老板查看指定月份的员工工资明细。
 * 查询工资记录表和员工表的关联数据。
 * 
 * @return int 查询成功返回1，失败返回0
 * 
 * 输入格式：YYYY-MM（如2024-01）
 * 显示信息：工号、姓名、部门、基本工资、奖金、扣款、实发工资
 * 还会计算并显示该月的工资总额
 */
int view_monthly_salary() {
    // 权限检查：只有老板可以查看月度工资
    if (current_user.role != ROLE_BOSS) {
        printf("权限不足！只有老板可以查看月度工资信息。\n");
        return 0;
    }
    
    MYSQL *conn;            // 数据库连接对象
    MYSQL_RES *result;      // 查询结果集
    MYSQL_ROW row;          // 结果集中的一行数据
    char query[1024];       // SQL查询语句缓冲区
    char month[8];          // 月份字符串，格式YYYY-MM
    
    printf("\n=== 月度工资查询 ===\n");
    printf("请输入月份(YYYY-MM): ");
    
    // 获取月份输入
    if (scanf("%7s", month) != 1) {
        printf("输入错误！\n");
        clear_input_buffer();
        return 0;
    }
    clear_input_buffer();
    
    // 连接到数据库
    conn = connect_to_database();
    if (conn == NULL) {
        return 0;
    }
    
    /**
     * 构建复杂查询语句：
     * 1. 关联salary_records和employees表
     * 2. 使用DATE_FORMAT函数匹配月份
     * 3. 按部门和工号排序
     */
    sprintf(query, "SELECT e.employee_id, e.name, e.department, s.base_salary, s.bonus, s.deduction, s.actual_salary FROM salary_records s JOIN employees e ON s.employee_id = e.employee_id WHERE DATE_FORMAT(s.salary_month, '%%Y-%%m') = '%s' ORDER BY e.department, e.employee_id", month);
    
    // 执行查询
    if (mysql_query(conn, query)) {
        fprintf(stderr, "查询失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    // 获取查询结果
    result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "存储结果失败: %s\n", mysql_error(conn));
        close_database(conn);
        return 0;
    }
    
    // 显示工资明细表头
    printf("\n%s月份工资明细:\n", month);
    printf("%-10s %-15s %-15s %-10s %-10s %-10s %-12s\n", 
           "工号", "姓名", "部门", "基本工资", "奖金", "扣款", "实发工资");
    printf("--------------------------------------------------------------------\n");
    
    // 遍历结果集，计算工资总额
    double total_salary = 0;
    int count = 0;
    while ((row = mysql_fetch_row(result)) != NULL) {
        printf("%-10s %-15s %-15s %-10s %-10s %-10s %-12s\n", 
               row[0] ? row[0] : "NULL",      // 工号
               row[1] ? row[1] : "NULL",      // 姓名
               row[2] ? row[2] : "NULL",      // 部门
               row[3] ? row[3] : "NULL",      // 基本工资
               row[4] ? row[4] : "NULL",      // 奖金
               row[5] ? row[5] : "NULL",      // 扣款
               row[6] ? row[6] : "NULL");     // 实发工资
        
        // 累加实发工资计算总额
        if (row[6]) {
            total_salary += atof(row[6]);
        }
        count++;
    }
    
    // 显示工资汇总信息
    printf("\n%s月份工资汇总:\n", month);
    printf("员工总数: %d\n", count);
    printf("工资总额: %.2f\n", total_salary);
    
    // 释放结果集和关闭连接
    mysql_free_result(result);
    close_database(conn);
    return 1;
}

/**
 * @brief 根据用户角色显示相应的功能菜单
 * 
 * 此函数根据当前登录用户的角色显示不同的菜单选项。
 * 每个角色只能看到和访问自己有权限的功能。
 * 
 * 菜单结构：
 * - 公共功能（所有角色）：查看员工信息、按条件查询
 * - 老板专属：添加员工、修改员工、删除员工、月度工资查询、初始化数据库
 * - 主管专属：修改员工信息
 * - 员工专属：无额外功能
 */
void display_menu_by_role() {
    // 显示系统标题和当前用户信息
    printf("\n==========================\n");
    printf("   员工信息管理系统\n");
    printf("   当前用户: %s", current_user.username);
    

    // 显示用户角色信息
    switch (current_user.role) {
        case ROLE_BOSS:
            printf(" [老板]");
            break;
        case ROLE_MANAGER:
            printf(" [主管 - %s]", current_user.department);
            break;
        case ROLE_EMPLOYEE:
            printf(" [员工]");
            break;
    }
    
    printf("\n==========================\n");
    
    // 公共菜单项 - 所有角色都可以访问
    printf("1. 查看员工信息\n");
    printf("2. 按部门/工号查询\n");
    
    // 根据角色显示不同的专属菜单项
    switch (current_user.role) {
        case ROLE_BOSS:
            // 老板菜单 - 所有管理功能
            printf("3. 添加新员工\n");
            printf("4. 修改员工信息\n");
            printf("5. 删除员工信息\n");
            printf("6. 月度工资查询\n");
            printf("7. 初始化数据库\n");
            printf("8. 退出系统\n");
            printf("请选择操作 [1-8]: ");
            break;
            
        case ROLE_MANAGER:
            // 主管菜单 - 部门管理功能
            printf("3. 修改员工信息\n");
            printf("4. 退出系统\n");
            printf("请选择操作 [1-4]: ");
            break;
            
        case ROLE_EMPLOYEE:
            // 员工菜单 - 只有查看功能
            printf("3. 退出系统\n");
            printf("请选择操作 [1-3]: ");
            break;
    }
}

/**
 * @brief 程序主函数
 * 
 * 程序的入口点，负责：
 * 1. 初始化MySQL客户端库
 * 2. 用户登录验证
 * 3. 显示主菜单循环
 * 4. 处理用户输入和执行相应功能
 * 5. 程序退出前的资源清理
 * 
 * @return int 程序退出状态码（0表示正常退出）
 * 
 * 程序流程：
 * 初始化 → 登录 → 主循环 → 清理退出
 */
int main() {
    int choice;     // 存储用户菜单选择
    
    // 初始化MySQL客户端库
    if (mysql_library_init(0, NULL, NULL)) {
        fprintf(stderr, "错误：无法初始化MySQL客户端库\n");
        return 1;
    }
    
    // 显示程序启动信息
    printf("员工信息管理系统 (MySQL版) 启动...\n");
    
    // 用户登录，如果登录失败则退出程序
    if (!login_system()) {
        mysql_library_end();
        return 1;
    }
    
    // 主程序循环 - 显示菜单并处理用户选择
    while (1) {
        // 根据用户角色显示相应的菜单
        display_menu_by_role();
        
        // 获取用户输入，验证输入是否为数字
        if (scanf("%d", &choice) != 1) {
            printf("输入错误，请输入数字！\n");
            clear_input_buffer();
            continue;  // 输入错误，重新显示菜单
        }
        clear_input_buffer();  // 清理输入缓冲区
        
        // 根据用户角色和选择执行相应的功能
        switch (current_user.role) {
            case ROLE_BOSS:
                // 老板的功能选项处理
                switch (choice) {
                    case 1:
                        view_employees_by_role();           // 查看员工信息
                        break;
                    case 2:
                        view_employees_by_department_or_id(); // 按条件查询
                        break;
                    case 3:
                        add_employee();                     // 添加新员工
                        break;
                    case 4:
                        update_employee();                  // 修改员工信息
                        break;
                    case 5:
                        delete_employee();                  // 删除员工信息
                        break;
                    case 6:
                        view_monthly_salary();              // 月度工资查询
                        break;
                    case 7:
                        initialize_database();              // 初始化数据库
                        break;
                    case 8:
                        printf("谢谢使用！再见！\n");       // 退出系统
                        mysql_library_end();
                        return 0;
                    default:
                        printf("输入错误，请重新选择。\n"); // 无效输入
                        break;
                }
                break;
                
            case ROLE_MANAGER:
                // 主管的功能选项处理
                switch (choice) {
                    case 1:
                        view_employees_by_role();           // 查看员工信息
                        break;
                    case 2:
                        view_employees_by_department_or_id(); // 按条件查询
                        break;
                    case 3:
                        update_employee();                  // 修改员工信息
                        break;
                    case 4:
                        printf("谢谢使用！再见！\n");       // 退出系统
                        mysql_library_end();
                        return 0;
                    default:
                        printf("输入错误，请重新选择。\n"); // 无效输入
                        break;
                }
                break;
                
            case ROLE_EMPLOYEE:
                // 员工的功能选项处理
                switch (choice) {
                    case 1:
                        view_employees_by_role();           // 查看员工信息
                        break;
                    case 2:
                        view_employees_by_department_or_id(); // 按条件查询
                        break;
                    case 3:
                        printf("谢谢使用！再见！\n");       // 退出系统
                        mysql_library_end();
                        return 0;
                    default:
                        printf("输入错误，请重新选择。\n"); // 无效输入
                        break;
                }
                break;
        }
    }
    
    // 清理MySQL客户端库资源（正常情况下不会执行到这里）
    mysql_library_end();
    return 0;
}