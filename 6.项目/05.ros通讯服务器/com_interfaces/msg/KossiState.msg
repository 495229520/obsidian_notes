int32[35] mx_state
# DD电机
float32 position_cdhd
float32 velocity_cdhc
float32 cdhd_acc
float32 cdhd_dec
float32 cdhd_jerk
float32 actpos_cdhd
uint16 errorid_cdhd
float32 homeposition_cdhc

# 上料机械爪脉冲
uint32 dspdset_1
int32 dposset_1
uint32 m_spd_1
int32 m_pos_relative_1

# 下料机械爪脉冲
uint32 dspdset_2
int32 dposset_2
uint32 m_spd_2
int32 m_pos_relative_2

# 行程
int32 clawposup_1
int32 clawposup_2
int32 clawposup_3
int32 clawposdown_1
int32 clawposdown_2
int32 clawposdown_3
