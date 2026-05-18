int32 robot_index # 多机械臂需通过编号来区别
string robot_name # 表示机械臂的作用或任务
string robot_type # 机械臂型号
string robot_soft_index # 机械臂软件版本
string robot_ip # 机器人的IP号

int32 curtask_index # 当前任务
int32 curstep_index # 当前任务的执行步骤

bool robot_en # 是否上使能
uint8 robot_mode # 机器人模式 手动或者自动
uint8[2] robot_speed # 机器人速度 0-手动速度,1-自动速度
float64[6] jt_cur_pos # 关节位置
float64[6] tl_cur_pos # 笛卡尔位置

uint32 robot_dgt_input # 控制箱数字量IO输入0-31
uint32 robot_dgt_output # 控制箱数字量IO输出0-31






