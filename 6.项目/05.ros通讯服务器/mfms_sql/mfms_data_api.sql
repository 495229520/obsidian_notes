-- MySQL dump 10.13  Distrib 8.0.43, for Win64 (x86_64)
--
-- Host: 127.0.0.1    Database: mfms_data
-- ------------------------------------------------------
-- Server version	8.0.43

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `device`
--

DROP TABLE IF EXISTS `device`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `device` (
  `id` varchar(10) NOT NULL COMMENT '设备唯一辨识符 type_3 + module_3 + id_4',
  `address` varchar(50) DEFAULT NULL COMMENT '设备地址',
  `create_ts` bigint DEFAULT NULL COMMENT '创建时间',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `device`
--

LOCK TABLES `device` WRITE;
/*!40000 ALTER TABLE `device` DISABLE KEYS */;
INSERT INTO `device` (`id`, `address`, `create_ts`) VALUES ('ctrkos0001','192.168.56.5',1764491402000),('rbafra0001','192.168.56.3',1764491402000),('rbthsu0001','192.168.56.4',1764491402000);
/*!40000 ALTER TABLE `device` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `device_state`
--

DROP TABLE IF EXISTS `device_state`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `device_state` (
  `id` varchar(10) NOT NULL COMMENT '设备唯一辨识符 type_3 + module_3 + id_4',
  `state` enum('offline','online','load','unload','connected') DEFAULT 'unload' COMMENT '设备状态',
  `info` json NOT NULL COMMENT '设备详细信息（JSON）',
  `err_code` int DEFAULT NULL COMMENT '错误码',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='设备信息';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `device_state`
--

LOCK TABLES `device_state` WRITE;
/*!40000 ALTER TABLE `device_state` DISABLE KEYS */;
INSERT INTO `device_state` (`id`, `state`, `info`, `err_code`) VALUES ('agvser0001','unload','{\"id\": \"0001\", \"ts\": \"1732951200020\", \"msg\": \"\", \"type\": \"agv\", \"state\": {\"pose\": {\"angle\": 1.57, \"pose_x\": 12.5, \"pose_y\": 8.8, \"confidence\": 0.99}, \"childDev\": [], \"velocity\": {\"vel_x\": 0.8, \"vel_y\": 0.0, \"vel_ang\": \"0.1\"}, \"run_stats\": {\"odo\": 1200.5, \"time\": 360000.0, \"today_odo\": 50.2, \"total_time\": 7200000.0}, \"navigation\": {\"last_station\": \"ST_00\", \"current_station\": \"ST_01\"}, \"controller_env\": {\"controller_humi\": 60.0, \"controller_temp\": 45.2, \"controller_voltage\": 24.5}, \"exception_status\": {\"slowed\": 1, \"blocked\": 0, \"slow_reason\": \"Corner\", \"block_reason\": \"None\"}}, \"module\": \"ser\", \"err_code\": \"0\"}',NULL),('ctrkos0001','unload','{\"id\": \"0001\", \"ts\": \"1732951200050\", \"msg\": \"\", \"type\": \"ctr\", \"state\": {\"childDev\": [], \"cdhd_params\": {\"cdhd_acc\": 500.0, \"cdhd_dec\": 500.0, \"cdhd_jerk\": 1000.0, \"actpos_cdhd\": 1023.8, \"errorid_cdhd\": 0, \"position_cdhd\": 1024.0, \"velocity_cdhc\": 100.0, \"homeposition_cdhc\": 0.0}, \"claw_status\": {\"clawposup\": [100, 200, 300], \"clawposdown\": [10, 20, 30]}, \"main_status\": {\"mx_state\": \"OperationEnabled\"}, \"settings_group_1\": {\"m_spd_1\": 50, \"dposset_1\": 5000, \"dspdset_1\": 100, \"m_pos_relative_1\": 200}, \"settings_group_2\": {\"m_spd_2\": 60, \"dposset_2\": 8000, \"dspdset_2\": 150, \"m_pos_relative_2\": 300}}, \"module\": \"kos\", \"err_code\": \"0\"}',NULL),('rbafra0001','unload','{\"id\": \"0001\", \"ts\": \"1732951200000\", \"msg\": \"Normal\", \"type\": \"rbt\", \"state\": {\"safety\": {\"emergency_stop\": 0}, \"childDev\": [], \"io_status\": {\"cl_dgt_input_h\": 0, \"cl_dgt_input_l\": 15, \"tl_dgt_input_l\": 0, \"cl_analog_input\": [0.5, 0.0], \"cl_dgt_output_h\": 0, \"cl_dgt_output_l\": 255, \"tl_dgt_output_l\": 1, \"cl_analog_output\": [10.0, 5.0]}, \"task_status\": {\"curstep_index\": 5, \"curtask_index\": 10, \"program_state\": 1, \"robot_motion_done\": 1}, \"error_status\": {\"sub_code\": 0, \"main_code\": 0, \"robot_err_code\": 0}, \"motion_status\": {\"jt_cur_pos\": [0.0, -90.0, 90.0, -90.0, 90.0, 0.0], \"robot_mode\": 1, \"tl_cur_pos\": [400.5, 0.0, 500.2, 180.0, 0.0, 0.0], \"robot_speed\": 80.5}}, \"module\": \"fra\", \"err_code\": \"0\"}',NULL),('rbthsu0001','unload','{\"id\": \"0001\", \"ts\": \"1732951200080\", \"msg\": \"\", \"type\": \"rbt\", \"state\": {\"childDev\": [], \"io_status\": {\"robot_dgt_input\": 12, \"robot_dgt_output\": 8}, \"task_status\": {\"curstep_index\": 10, \"curtask_index\": 2}, \"motion_status\": {\"robot_en\": 1, \"auto_speed\": 100, \"jt_cur_pos\": [0.1, 0.2, 0.3, 0.4, 0.5, 0.6], \"robot_mode\": 1, \"tl_cur_pos\": [100.0, 200.0, 300.0, 0.0, 90.0, 0.0], \"manual_speed\": 20}}, \"module\": \"hsu\", \"err_code\": \"0\"}',NULL);
/*!40000 ALTER TABLE `device_state` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `hyrms_log`
--

DROP TABLE IF EXISTS `hyrms_log`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `hyrms_log` (
  `id` bigint NOT NULL AUTO_INCREMENT,
  `timestamp` bigint NOT NULL,
  `severity` varchar(16) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL,
  `name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL,
  `msg` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL,
  `file` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL,
  `function` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL,
  `line` int NOT NULL,
  PRIMARY KEY (`id`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `hyrms_log`
--

LOCK TABLES `hyrms_log` WRITE;
/*!40000 ALTER TABLE `hyrms_log` DISABLE KEYS */;
/*!40000 ALTER TABLE `hyrms_log` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `lua_script`
--

DROP TABLE IF EXISTS `lua_script`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `lua_script` (
  `script_name` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT 'lua脚本名称',
  `script_content` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci COMMENT '脚本内容',
  `comments` varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci DEFAULT NULL COMMENT '备注',
  `state` enum('running','ready','wait','pause','stop') CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL DEFAULT 'wait' COMMENT '状态信息',
  PRIMARY KEY (`script_name`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci ROW_FORMAT=DYNAMIC COMMENT='lua脚本表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `lua_script`
--

LOCK TABLES `lua_script` WRITE;
/*!40000 ALTER TABLE `lua_script` DISABLE KEYS */;
INSERT INTO `lua_script` (`script_name`, `script_content`, `comments`, `state`) VALUES (' loadlib','package.cpath = package.cpath .. \";install/lua_interface/lib/mylib.so\" \r\n-- 尝试加载C++库\r\nmylib = require(\"mylib\")',NULL,'wait'),('!!!危险!!!','!XXX!脚本操控了现实设备，确保完全理解代码再载入',NULL,'wait'),('!FlashTest!','--危险！理解后执行\nfunction sleep(n)\nos.execute(\"sleep \" .. tonumber(n))\nend\n\r\nprint(flash.pwmFlash.connect(\"Flash1\").err.msg)\nfor i = 1, 20 do\n		print(flash.pwmFlash.setFreq(\"Flash1\",60).err.msg)\n		print(\"频率目前为\"..flash.pwmFlash.getFreq(\"Flash1\").freq..\"fpm\")\n		print(flash.pwmFlash.setFreq(\"Flash1\",0).err.msg)\n		print(\"频率目前为\"..flash.pwmFlash.getFreq(\"Flash1\").freq..\"fpm\")\nend\r\nprint(flash.pwmFlash.disConnect(\"Flash1\").err.msg)\n',NULL,'wait'),('!FlashTest2!','--危险！理解后执行\nfunction sleep(n)\n    os.execute(\"sleep \" .. tonumber(n))\nend\n\n-- 连接设备\nlocal res = flash.pwmFlash.connect(\"Flash1\")\nprint(\"connect: \" .. res.err.msg)\n\n-- 先连续加速 20 次\nfor i = 1, 20 do\n    local res_step = flash.pwmFlash.stepCtrl(\"Flash1\", flash.FASTER)\n    print(string.format(\"Step FASTER #%d: %s\", i, res_step.err.msg))\n--    sleep(0.3)\nend\n\n-- 再连续减速 20 次\nfor i = 1, 20 do\n    local res_step = flash.pwmFlash.stepCtrl(\"Flash1\", flash.SLOWER)\n    print(string.format(\"Step SLOWER #%d: %s\", i, res_step.err.msg))\n--    sleep(0.3)\nend\n\n-- 断开设备\nres = flash.pwmFlash.disConnect(\"Flash1\")\nprint(\"disconnect: \" .. res.err.msg)\n',NULL,'wait'),('!Fr434DescTest!','--危险！理解后执行\r\nfunction sleep(n)\r\nos.execute(\"sleep \" .. tonumber(n))\r\nend\r\n\r\nprint(\"Start\")\r\nd = mylib.GetDescPose(\"Fr1\");\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\n\r\n--x轴测试\r\nsleep(3)\r\nd[1] = d[1] + 20\r\nmylib.PTP(\"Fr1\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[1]+20 !\")\r\n\r\nsleep(3)\r\nd[1] = d[1] - 20\r\nmylib.PTP(\"Fr1\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[1]-20 !\")\r\n\r\n--y轴测试\r\nsleep(3)\r\nd[2] = d[2] + 20\r\nmylib.PTP(\"Fr1\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[2]+20 !\")\r\n\r\n\r\nsleep(3)\r\nd[2] = d[2] - 20\r\nmylib.PTP(\"Fr1\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[2]-20 !\")\r\n\r\n\r\n--z轴测试\r\nsleep(3)\r\nd[3] = d[3] + 20\r\nmylib.PTP(\"Fr1\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[3]+20 !\")\r\n\r\n\r\nsleep(3)\r\nd[3] = d[3] - 20\r\nmylib.PTP(\"Fr1\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[3]-20 !\")\r\n\r\nprint(\"test pass x,y,z\")',NULL,'wait'),('!Fr434JTest!','--危险！理解后执行\r\nfunction sleep(n)\r\nos.execute(\"sleep \" .. tonumber(n))\r\nend\r\n\r\nprint(\"Start\")\r\nj = mylib.GetJointPose(\"Fr1\");\r\nfor k, v in ipairs(j) do\r\n	print(k..\":\"..v)\r\nend\r\n\r\nj[1] = j[1] + 10\r\nmylib.PTP(\"Fr1\",JOINT,j);\r\nfor k, v in ipairs(j) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved j[1]+10!\")\r\n\r\nsleep(3)\r\n\r\nj[1] = j[1] - 10\r\nmylib.PTP(\"Fr1\",JOINT,j);\r\nfor k, v in ipairs(j) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved j[1]-10!\")',NULL,'wait'),('!frtest!','--危险！理解后执行\r\npackage.cpath = package.cpath .. \";/home/mightning/MFMS_test/install/lua_interface/lib/mylib.so\" \n-- 尝试加载C++库\nlocal mylib = require(\"mylib\")\nmylib.Connect(\"Fr1\");\nj = mylib.GetJointPose(\"Fr1\");\nmylib.PTP(\"Fr1\",JOINT,j);\n\n\n\nmylib.SetMode(\"Fr1\",AUTO_MODE);\n',NULL,'wait'),('!NewFrTest!','--危险！理解后执行\r\nfunction sleep(n)\r\nos.execute(\"sleep \" .. tonumber(n))\r\nend\r\n\r\nprint(\"Start\")\r\nd = robot.Fr.GetDescPose(\"Fr71\");\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\n\r\n--x轴测试\r\nsleep(3)\r\nd[1] = d[1] + 20\r\nrobot.Fr.PTP(\"Fr71\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[1]+20 !\")\r\n\r\nsleep(3)\r\nd[1] = d[1] - 20\r\nrobot.Fr.PTP(\"Fr71\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[1]-20 !\")\r\n\r\n--y轴测试\r\nsleep(3)\r\nd[2] = d[2] + 20\r\nrobot.Fr.PTP(\"Fr71\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[2]+20 !\")\r\n\r\n\r\nsleep(3)\r\nd[2] = d[2] - 20\r\nrobot.Fr.PTP(\"Fr71\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[2]-20 !\")\r\n\r\n\r\n--z轴测试\r\nsleep(3)\r\nd[3] = d[3] + 20\r\nrobot.Fr.PTP(\"Fr71\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[3]+20 !\")\r\n\r\n\r\nsleep(3)\r\nd[3] = d[3] - 20\r\nrobot.Fr.PTP(\"Fr71\",DESC,d);\r\nfor k, v in ipairs(d) do\r\n	print(k..\":\"..v)\r\nend\r\nprint(\"! moved d[3]-20 !\")\r\n\r\nprint(\"test pass x,y,z\")',NULL,'wait'),('?agvtest?','package.cpath = package.cpath .. \";/home/mightning/MFMS_test/install/lua_interface/lib/mylib.so\" \n-- 尝试加载C++库\nlocal mylib = require(\"mylib\")\nmylib.Connect(\'Xg1\');\nmylib.Agv_MoveLinear(\'Xg1\',1.0 ,0.5, 0.0, 0);',NULL,'wait'),('?yolotest?','package.cpath = package.cpath .. \";/home/mightning/MFMS_test/install/lua_interface/lib/mylib.so\" \n-- 尝试加载C++库\nlocal mylib = require(\"mylib\")\nobjecttable = Yolo_GetState(\"Yo1\");\nprint(objecttable[1].class_name);\nfor i, obj in ipairs(objecttable) do\n    print(string.format(\"\\nObject %d:\", i))\n    print(string.format(\"  Class: %s\", obj.class_name))\n    print(string.format(\"  Confidence: %.2f%%\", obj.confidence * 100))  -- 假设confidence是0-1范围\n    print(string.format(\"  Position: (%.2f, %.2f, %.2f)\", obj.x, obj.y, obj.z))\nend',NULL,'wait'),('FrConn','mylib.Connect(\"Fr1\");',NULL,'wait'),('FrDisConn','mylib.DisConnect(\"Fr1\")',NULL,'wait'),('FrGetJ','j = mylib.GetJointPose(\"Fr1\");\r\nfor k, v in ipairs(j) do\r\n	print(v)\r\nend',NULL,'wait'),('NewFrConn','robot.Fr.Connect(\"Fr71\")',NULL,'wait'),('ttttest','ttt.tttDev.connect(\"Ttt1\")\r\n',NULL,'wait'),('virttest','print(virt.virtDev.connect(\"Virt1\").msg);',NULL,'wait');
/*!40000 ALTER TABLE `lua_script` ENABLE KEYS */;
UNLOCK TABLES;
/*!50003 SET @saved_cs_client      = @@character_set_client */ ;
/*!50003 SET @saved_cs_results     = @@character_set_results */ ;
/*!50003 SET @saved_col_connection = @@collation_connection */ ;
/*!50003 SET character_set_client  = utf8mb4 */ ;
/*!50003 SET character_set_results = utf8mb4 */ ;
/*!50003 SET collation_connection  = utf8mb4_0900_ai_ci */ ;
/*!50003 SET @saved_sql_mode       = @@sql_mode */ ;
/*!50003 SET sql_mode              = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION' */ ;
DELIMITER ;;
/*!50003 CREATE*/ /*!50017 DEFINER=`root`@`localhost`*/ /*!50003 TRIGGER `lua_update_trigger` AFTER UPDATE ON `lua_script` FOR EACH ROW begin
    IF NEW.state = 'ready' THEN
        UPDATE trigger_table
        SET trigger_value = true
        where trigger_name = 'lua_start';
    end IF;
end */;;
DELIMITER ;
/*!50003 SET sql_mode              = @saved_sql_mode */ ;
/*!50003 SET character_set_client  = @saved_cs_client */ ;
/*!50003 SET character_set_results = @saved_cs_results */ ;
/*!50003 SET collation_connection  = @saved_col_connection */ ;

--
-- Table structure for table `users`
--

DROP TABLE IF EXISTS `users`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `users` (
  `name` varchar(30) NOT NULL,
  `password` varchar(30) NOT NULL,
  `permission` tinyint NOT NULL,
  PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `users`
--

LOCK TABLES `users` WRITE;
/*!40000 ALTER TABLE `users` DISABLE KEYS */;
/*!40000 ALTER TABLE `users` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `trigger_table`
--

DROP TABLE IF EXISTS `trigger_table`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `trigger_table` (
  `trigger_name` varchar(100) NOT NULL COMMENT '触发器名称',
  `trigger_value` tinyint NOT NULL DEFAULT '0' COMMENT '触发器值（0=未触发，1=已触发）',
  PRIMARY KEY (`trigger_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='系统触发器表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `trigger_table`
--

LOCK TABLES `trigger_table` WRITE;
/*!40000 ALTER TABLE `trigger_table` DISABLE KEYS */;
INSERT INTO `trigger_table` (`trigger_name`, `trigger_value`) VALUES
('device_changed', 0),
('lua_start', 0),
('lua_pause', 0);
/*!40000 ALTER TABLE `trigger_table` ENABLE KEYS */;
UNLOCK TABLES;

/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2025-11-30 19:51:42
