# 小车信息
string vehicle_id # 小车id
string vehicle_name # 小车名称
string version # Robokit版本号
string current_ip # 当前ip地址
# 小车运行信息
float64 odo # 累计行驶里程,单位:m
float64 today_odo # 今日累计行驶里程,单位:m
float64 time # 本次运行时间(从开机到现在),单位:ms
float64 total_time # 累计运行时间,单位:ms
float64 controller_temp # 控制器温度,单位:度
float64 controller_humi # 控制器湿度,单位:%
float64 controller_voltage # 控制器电压,单位:V
# 小车位置
float64 pose_x # 小车的x坐标
float64 pose_y # 小车的y坐标
float64 angle # 小车的angle坐标
float64 confidence # 小车的定位可信度
string current_station # 离小车最近站点的id
string last_station # 小车上一个站点的id
# 小车的速度
float64 vel_x # 小车在x方向上的速度,单位:m/s
float64 vel_y # 小车在y方向上的速度,单位:m/s
float64 vel_ang # 小车的角速度,顺时针为正,单位:rad/s
# 小车是否被阻挡
bool blocked # 小车是否被阻挡
string block_reason # 小车被阻挡的原因
bool slowed # 小车是否被减速
string slow_reason # 小车被减速的原因
# 小车报警
bool has_err # 是否报错
string err_level # 报错等级
string err_json # 报错原因
