# Robot status message
# Published by each robot at ~10Hz

string robot_id
string robot_type
int64 timestamp_ns
uint8[] payload  # Type-specific payload (JSON or binary)
