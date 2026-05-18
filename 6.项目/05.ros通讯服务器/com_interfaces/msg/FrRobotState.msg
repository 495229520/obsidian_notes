int32 robot_index # 多机械臂需通过编号来区别
string robot_name # 表示机械臂的作用或任务
string robot_type # 机械臂型号
string robot_soft_index # 机械臂软件版本
string robot_ip # 机器人的IP号

int32 curtask_index # 当前任务
int32 curstep_index # 当前任务的执行步骤
uint8 program_state # 程序运行状态
uint8 robot_motion_done # 运动状态
uint8 robot_err_code # 错误码
int32 main_code # 主故障码
int32 sub_code # 子故障码
uint8 robot_mode # 机器人模式 手动或者自动
float64 robot_speed # 机器人速度 暂时无法获取
float64[6] jt_cur_pos # 关节位置
float64[6] tl_cur_pos # 笛卡尔位置
uint8 cl_dgt_output_h # 控制箱数字量IO输出15-8
uint8 cl_dgt_output_l # 控制箱数字量IO输出7-0
uint8 cl_dgt_input_h # 控制箱数字量IO输入15-8
uint8 cl_dgt_input_l # 控制箱数字量IO输入7-0
uint16[2] cl_analog_input # 控制箱模拟输入AI
uint16[2] cl_analog_output # 控制箱模拟输出AO
uint8 tl_dgt_output_l # 工具数字量IO输出7-0 仅bit0和bit1
uint8 tl_dgt_input_l # 工具数字量IO输入7-0 仅bit0和bit1

uint8 emergency_stop # 急停标志
