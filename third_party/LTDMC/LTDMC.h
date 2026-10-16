#ifndef _DMC_LIB_H
#define _DMC_LIB_H

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#ifndef NULL
    #ifdef __cplusplus
        #define NULL 0
    #else
        #define NULL ((void *)0)
    #endif
#endif

typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef float FLOAT;

typedef unsigned char uint8;   /* defined for unsigned 8-bits integer variable 	无符号8位整型变量  */
typedef signed char int8;      /* defined for signed 8-bits integer variable		有符号8位整型变量  */
typedef unsigned short uint16; /* defined for unsigned 16-bits integer variable 	无符号16位整型变量 */
typedef signed short int16;    /* defined for signed 16-bits integer variable 		有符号16位整型变量 */
typedef unsigned int uint32;   /* defined for unsigned 32-bits integer variable 	无符号32位整型变量 */
typedef signed int int32;      /* defined for signed 32-bits integer variable 		有符号32位整型变量 */
typedef unsigned long long uint64;
typedef signed long long int64;

typedef uint32(__stdcall *DMC3K5K_OPERATE)(void *operator_data);

typedef struct
{
    uint32 m_Time;
    int32 m_CommandPos;
    double m_CommandVel;
    uint32 m_CommandAcc;
    int32 m_FpgaPos;
    double m_FpgaVel;
    int32 m_EncoderPos;
    double m_ErrorPos;
} struct_PidAdjustData;

typedef struct
{
    double start_pos; //线性比较起始点位置.
    double interval;  //间距.
    int count;        //个数
} struct_hs_cmp_info;


#define __DMC_EXPORTS

//定义输入和输出
#ifdef __DMC_EXPORTS
    #define DMC_API __declspec(dllexport)
#else
    #define DMC_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

//设置和读取打印模式
DMC_API short __stdcall dmc_set_debug_mode(WORD mode, const char *pFileName);
DMC_API short __stdcall dmc_get_debug_mode(WORD *mode, char *pFileName);

DMC_API short __stdcall dmc_board_init(void);                 //初始化控制卡
DMC_API short __stdcall dmc_board_init_onecard(WORD CardNo);  ////客户定制独立开卡 初始化一张控制卡
DMC_API short __stdcall dmc_board_close_onecard(WORD CardNo); ////客户定制独立关卡 关闭一个控制卡
DMC_API short __stdcall dmc_get_CardInfList(WORD *CardNum, DWORD *CardTypeList, WORD *CardIdList); //读取控制卡信息列表
DMC_API short __stdcall dmc_board_close(void);                                                     //关闭控制卡
DMC_API short __stdcall dmc_board_reset(void);                                                     //硬件复位
DMC_API short __stdcall dmc_board_reset_onecard(WORD CardNo);                                      //客户定制单独硬件复位
DMC_API short __stdcall dmc_soft_reset(WORD CardNo);                                //控制卡软复位（pci卡） 热复位（总线卡）
DMC_API short __stdcall dmc_cool_reset(WORD CardNo);                                //控制卡冷复位
DMC_API short __stdcall dmc_original_reset(WORD CardNo);                            //控制卡初始复位
DMC_API short __stdcall dmc_get_card_ID(WORD CardNo, DWORD *CardID);                //读取控制卡卡号
DMC_API short __stdcall dmc_get_release_version(WORD CardNo, char *ReleaseVersion); //读取发布版本号
DMC_API short __stdcall dmc_get_card_version(WORD CardNo, DWORD *CardVersion);      //读取控制卡硬件版本
DMC_API short __stdcall dmc_get_card_soft_version(WORD CardNo, DWORD *FirmID, DWORD *SubFirmID); //读取控制卡硬件的固件版本
DMC_API short __stdcall dmc_get_card_lib_version(DWORD *LibVer);                                 //读取控制卡动态库版本
DMC_API short __stdcall dmc_get_total_axes(WORD CardNo, DWORD *TotalAxis);                       //读取指定卡轴数
DMC_API short __stdcall dmc_get_total_liners(WORD CardNo, DWORD *TotalLiner);                    //读取指定卡插补坐标系数

DMC_API short __stdcall dmc_get_total_ionum(WORD CardNo, WORD *TotalIn, WORD *TotalOut);  //获取本地IO点数
DMC_API short __stdcall dmc_get_total_adcnum(WORD CardNo, WORD *TotalIn, WORD *TotalOut); //获取本地ADDA输入输出数

//密码函数
DMC_API short __stdcall dmc_check_sn(WORD CardNo, const char *str_sn);
DMC_API short __stdcall dmc_write_sn(WORD CardNo, const char *str_sn);

/***********轴参数*************/
//脉冲模式
DMC_API short __stdcall dmc_set_pulse_outmode(WORD CardNo, WORD axis, WORD outmode);
DMC_API short __stdcall dmc_get_pulse_outmode(WORD CardNo, WORD axis, WORD *outmode);
//脉冲当量
DMC_API short __stdcall dmc_set_equiv(WORD CardNo, WORD axis, double equiv);
DMC_API short __stdcall dmc_get_equiv(WORD CardNo, WORD axis, double *equiv);
//反向间隙(当量)
DMC_API short __stdcall dmc_set_backlash_unit(WORD CardNo, WORD axis, double backlash);
DMC_API short __stdcall dmc_get_backlash_unit(WORD CardNo, WORD axis, double *backlash);
//反向间隙(脉冲)
DMC_API short __stdcall dmc_set_backlash(WORD CardNo, WORD axis, long backlash);
DMC_API short __stdcall dmc_get_backlash(WORD CardNo, WORD axis, long *backlash);

//加速度接口
DMC_API short __stdcall dmc_set_profile_unit_acc(WORD CardNo, WORD axis, double Min_Vel, double Max_Vel, double Tacc, double Tdec,
                                                 double Stop_Vel);
DMC_API short __stdcall dmc_get_profile_unit_acc(WORD CardNo, WORD axis, double *Min_Vel, double *Max_Vel, double *Tacc,
                                                 double *Tdec, double *Stop_Vel);
DMC_API short __stdcall dmc_set_vector_profile_multicoor_acc(WORD CardNo, WORD iDMC_ILINER, double Min_Vel, double Max_Vel,
                                                             double Acc, double Dec, double Stop_Vel);
DMC_API short __stdcall dmc_get_vector_profile_multicoor_acc(WORD CardNo, WORD iDMC_ILINER, double *Min_Vel, double *Max_Vel,
                                                             double *Acc, double *Dec, double *Stop_Vel);
DMC_API short __stdcall dmc_set_vector_profile_unit_acc(WORD CardNo, WORD Crd, double Min_Vel, double Max_Vel, double Tacc,
                                                        double Tdec, double Stop_Vel);
DMC_API short __stdcall dmc_get_vector_profile_unit_acc(WORD CardNo, WORD Crd, double *Min_Vel, double *Max_Vel, double *Tacc,
                                                        double *Tdec, double *Stop_Vel);
DMC_API short __stdcall dmc_change_speed_unit_acc(WORD CardNo, WORD axis, double New_Vel, double Taccdec);
DMC_API short __stdcall dmc_change_speed_acc(WORD CardNo, WORD axis, double Curr_Vel, double Taccdec);
DMC_API short __stdcall dmc_pmove_extern_acc(WORD CardNo, WORD axis, double dist, double Min_Vel, double Max_Vel, double Tacc,
                                             double Tdec, double stop_Vel, double s_para, WORD posi_mode);
DMC_API short __stdcall nmc_set_home_profile_acc(WORD CardNo, WORD axis, WORD home_mode, double Low_Vel, double High_Vel,
                                                 double Tacc, double Tdec, double offsetpos);
DMC_API short __stdcall nmc_get_home_profile_acc(WORD CardNo, WORD axis, WORD *home_mode, double *Low_Vel, double *High_Vel,
                                                 double *Tacc, double *Tdec, double *Offsetpos);
DMC_API short __stdcall dmc_set_home_profile_unit_acc(WORD CardNo, WORD axis, double Low_Vel, double High_Vel, double Tacc,
                                                      double Tdec); //设置回零速度参数
DMC_API short __stdcall dmc_get_home_profile_unit_acc(WORD CardNo, WORD axis, double *Low_Vel, double *High_Vel, double *Tacc,
                                                      double *Tdec); //读取回零速度参数
DMC_API short __stdcall dmc_t_pmove_extern_softstart_acc(WORD CardNo, WORD axis, double MidPos, double TargetPos,
                                                         double start_Vel, double Max_Vel, double stop_Vel, DWORD delay_ms,
                                                         double Max_Vel2, double stop_vel2, double acc_time, double dec_time,
                                                         WORD posi_mode);
/***********************************文件操作**************************************/
/*********************************************************************************************************
文件操作添加 总线卡
filetype
0-basic
1-gcode
2-setting
3-firewave
4-CAN configfile
100-trace data
*********************************************************************************************************/
DMC_API short __stdcall dmc_download_file(WORD CardNo, const char *pfilename, const char *pfilenameinControl, WORD filetype);
//下载内存文件 总线卡
DMC_API short __stdcall dmc_download_memfile(WORD CardNo, const char *pbuffer, uint32 buffsize, const char *pfilenameinControl,
                                             WORD filetype);
//上传文件
DMC_API short __stdcall dmc_upload_file(WORD CardNo, const char *pfilename, const char *pfilenameinControl, WORD filetype);
//上传内存文件
DMC_API short __stdcall dmc_upload_memfile(WORD CardNo, char *pbuffer, uint32 buffsize, const char *pfilenameinControl,
                                           uint32 *puifilesize, WORD filetype);
//下载参数文件
DMC_API short __stdcall dmc_download_configfile(WORD CardNo, const char *FileName);
//下载固件文件
DMC_API short __stdcall dmc_download_firmware(WORD CardNo, const char *FileName);
//文件进度
DMC_API short __stdcall dmc_get_progress(WORD CardNo, float *process);

//安全参数
DMC_API short __stdcall dmc_set_softlimit(WORD CardNo, WORD axis, WORD enable, WORD source_sel, WORD SL_action, long N_limit,
                                          long P_limit); //设置软限位参数
DMC_API short __stdcall dmc_get_softlimit(WORD CardNo, WORD axis, WORD *enable, WORD *source_sel, WORD *SL_action, long *N_limit,
                                          long *P_limit);                                                     //读取软限位参数
DMC_API short __stdcall dmc_set_el_mode(WORD CardNo, WORD axis, WORD enable, WORD el_logic, WORD el_mode);    //设置EL信号
DMC_API short __stdcall dmc_get_el_mode(WORD CardNo, WORD axis, WORD *enable, WORD *el_logic, WORD *el_mode); //读取设置EL信号
DMC_API short __stdcall dmc_set_emg_mode(WORD CardNo, WORD axis, WORD enable, WORD emg_logic);                //设置EMG信号
DMC_API short __stdcall dmc_get_emg_mode(WORD CardNo, WORD axis, WORD *enable, WORD *emg_logic);              //读取设置EMG信号

/*************************************单轴运动*****************************************/
//速度设置
DMC_API short __stdcall dmc_set_profile(WORD CardNo, WORD axis, double Min_Vel, double Max_Vel, double Tacc, double Tdec,
                                        double stop_vel); //设定速度曲线参数
DMC_API short __stdcall dmc_get_profile(WORD CardNo, WORD axis, double *Min_Vel, double *Max_Vel, double *Tacc, double *Tdec,
                                        double *stop_vel); //读取速度曲线参数
//速度设置(脉冲当量)
DMC_API short __stdcall dmc_set_profile_unit(WORD CardNo, WORD axis, double Min_Vel, double Max_Vel, double Tacc, double Tdec,
                                             double Stop_Vel);
DMC_API short __stdcall dmc_get_profile_unit(WORD CardNo, WORD axis, double *Min_Vel, double *Max_Vel, double *Tacc, double *Tdec,
                                             double *Stop_Vel);

//20160105增加新速度曲线以加速度 减速度 减减速度来表示(脉冲)
DMC_API short __stdcall dmc_set_profile_extern(WORD CardNo, WORD axis, double Min_Vel, double Max_Vel, double Acc, double Dec,
                                               double Ajerk, double Djerk, double stop_vel);
DMC_API short __stdcall dmc_get_profile_extern(WORD CardNo, WORD axis, double *Min_Vel, double *Max_Vel, double *Acc, double *Dec,
                                               double *Ajerk, double *Djerk, double *stop_vel);
//速度曲线设置，加速度值表示(脉冲)
DMC_API short __stdcall dmc_set_acc_profile(WORD CardNo, WORD axis, double Min_Vel, double Max_Vel, double Acc, double Dec,
                                            double stop_vel); //设定速度曲线参数
DMC_API short __stdcall dmc_get_acc_profile(WORD CardNo, WORD axis, double *Min_Vel, double *Max_Vel, double *Acc, double *Dec,
                                            double *stop_vel); //读取速度曲线参数
//速度曲线设置，加速度值表示(当量)
DMC_API short __stdcall dmc_set_profile_unit_acc(WORD CardNo, WORD axis, double Min_Vel, double Max_Vel, double Tacc, double Tdec,
                                                 double Stop_Vel);
DMC_API short __stdcall dmc_get_profile_unit_acc(WORD CardNo, WORD axis, double *Min_Vel, double *Max_Vel, double *Tacc,
                                                 double *Tdec, double *Stop_Vel);
DMC_API short __stdcall dmc_set_s_profile(WORD CardNo, WORD axis, WORD s_mode, double s_para); //设置平滑速度曲线参数
DMC_API short __stdcall dmc_get_s_profile(WORD CardNo, WORD axis, WORD s_mode,
                                          double *s_para); //读取平滑速度曲线参数 兼容DMC5800 s_mode改用指针返回参数值

//点位运动(脉冲)
DMC_API short __stdcall dmc_pmove(WORD CardNo, WORD axis, long dist, WORD posi_mode); //指定轴做定长位移运动
//点位运动(当量)
DMC_API short __stdcall dmc_pmove_unit(WORD CardNo, WORD axis, double Dist, WORD posi_mode);
//单轴连续速度运动
DMC_API short __stdcall dmc_vmove(WORD CardNo, WORD axis, WORD dir);
//指定轴做定长位移运动 同时发送速度和S时间(脉冲)
DMC_API short __stdcall dmc_pmove_extern(WORD CardNo, WORD axis, double dist, double Min_Vel, double Max_Vel, double Tacc,
                                         double Tdec, double stop_Vel, double s_para, WORD posi_mode);
//在线变位/变速(脉冲)
DMC_API short __stdcall dmc_reset_target_position(WORD CardNo, WORD axis, long dist, WORD posi_mode); //运动中改变目标位置
DMC_API short __stdcall dmc_change_speed(WORD CardNo, WORD axis, double Curr_Vel, double Taccdec); //在线改变指定轴的当前运动速度
DMC_API short __stdcall dmc_update_target_position(WORD CardNo, WORD axis, long dist,
                                                   WORD posi_mode); //无论运动与否强行改变目标位置
//变速变位(当量)
DMC_API short __stdcall dmc_reset_target_position_unit(WORD CardNo, WORD axis, double New_Pos);
DMC_API short __stdcall dmc_change_speed_unit(WORD CardNo, WORD axis, double New_Vel, double Taccdec);
DMC_API short __stdcall dmc_update_target_position_unit(WORD CardNo, WORD axis, double New_Pos);

/******************************插补运动**********************************/
//3000系列速度设置(脉冲)
DMC_API short __stdcall dmc_set_vector_profile_multicoor(WORD CardNo, WORD Crd, double Min_Vel, double Max_Vel, double Tacc,
                                                         double Tdec, double Stop_Vel);
DMC_API short __stdcall dmc_get_vector_profile_multicoor(WORD CardNo, WORD Crd, double *Min_Vel, double *Max_Vel, double *Tacc,
                                                         double *Tdec, double *Stop_Vel);
DMC_API short __stdcall dmc_set_vector_s_profile_multicoor(WORD CardNo, WORD Crd, WORD s_mode,
                                                           double s_para); //设置平滑速度曲线参数
DMC_API short __stdcall dmc_get_vector_s_profile_multicoor(WORD CardNo, WORD Crd, WORD s_mode,
                                                           double *s_para); //读取平滑速度曲线参数

//插补速度参数(当量)
DMC_API short __stdcall dmc_set_vector_profile_unit(WORD CardNo, WORD Crd, double Min_Vel, double Max_Vel, double Tacc,
                                                    double Tdec, double Stop_Vel);
DMC_API short __stdcall dmc_get_vector_profile_unit(WORD CardNo, WORD Crd, double *Min_Vel, double *Max_Vel, double *Tacc,
                                                    double *Tdec, double *Stop_Vel);
DMC_API short __stdcall dmc_set_vector_s_profile(WORD CardNo, WORD Crd, WORD s_mode, double s_para); //设置平滑速度曲线参数
DMC_API short __stdcall dmc_get_vector_s_profile(WORD CardNo, WORD Crd, WORD s_mode, double *s_para);

//3000系列插补函数(脉冲)
DMC_API short __stdcall dmc_line_multicoor(WORD CardNo, WORD Crd, WORD axisNum, WORD *axisList, long *DistList,
                                           WORD posi_mode); //指定轴直线插补运动
DMC_API short __stdcall dmc_arc_move_multicoor(WORD CardNo, WORD Crd, WORD *AxisList, long *Target_Pos, long *Cen_Pos,
                                               WORD Arc_Dir, WORD posi_mode); //圆弧插补运动
//单段插补(当量)
DMC_API short __stdcall dmc_line_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Dist, WORD posi_mode);
DMC_API short __stdcall dmc_arc_move_center_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                 double *Cen_Pos, WORD Arc_Dir, long Circle, WORD posi_mode);
DMC_API short __stdcall dmc_arc_move_radius_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                 double Arc_Radius, WORD Arc_Dir, long Circle, WORD posi_mode);
DMC_API short __stdcall dmc_arc_move_3points_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                  double *Mid_Pos, long Circle, WORD posi_mode);
DMC_API short __stdcall dmc_rectangle_move_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                double *Mark_Pos, long num, WORD rect_mode, WORD posi_mode);

/********************PVT运动****************************/
//PVT运动 旧版
DMC_API short __stdcall dmc_PvtTable(WORD CardNo, WORD iaxis, DWORD count, double *pTime, long *pPos, double *pVel);
DMC_API short __stdcall dmc_PtsTable(WORD CardNo, WORD iaxis, DWORD count, double *pTime, long *pPos, double *pPercent);
DMC_API short __stdcall dmc_PvtsTable(WORD CardNo, WORD iaxis, DWORD count, double *pTime, long *pPos, double velBegin,
                                      double velEnd);
DMC_API short __stdcall dmc_PttTable(WORD CardNo, WORD iaxis, DWORD count, double *pTime, long *pPos);
DMC_API short __stdcall dmc_PvtMove(WORD CardNo, WORD AxisNum, WORD *AxisList);
//PVT缓冲区添加
DMC_API short __stdcall dmc_PttTable_add(WORD CardNo, WORD iaxis, DWORD count, double *pTime, long *pPos);
DMC_API short __stdcall dmc_PtsTable_add(WORD CardNo, WORD iaxis, DWORD count, double *pTime, long *pPos, double *pPercent);
DMC_API short __stdcall dmc_pvt_get_remain_space(WORD CardNo, WORD iaxis); //读取pvt剩余空间
/*****************************************************************************
PVT运动 总线卡新规划
******************************************************************************/
DMC_API short __stdcall dmc_pvt_table_unit(WORD CardNo, WORD iaxis, DWORD count, double *pTime, double *pPos, double *pVel);
DMC_API short __stdcall dmc_pts_table_unit(WORD CardNo, WORD iaxis, DWORD count, double *pTime, double *pPos, double *pPercent);
DMC_API short __stdcall dmc_pvts_table_unit(WORD CardNo, WORD iaxis, DWORD count, double *pTime, double *pPos, double velBegin,
                                            double velEnd);
DMC_API short __stdcall dmc_ptt_table_unit(WORD CardNo, WORD iaxis, DWORD count, double *pTime, double *pPos);
DMC_API short __stdcall dmc_pvt_move(WORD CardNo, WORD AxisNum, WORD *AxisList);

DMC_API short __stdcall dmc_SetGearProfile(WORD CardNo, WORD axis, WORD MasterType, WORD MasterIndex, long MasterEven,
                                           long SlaveEven, DWORD MasterSlope);
DMC_API short __stdcall dmc_GetGearProfile(WORD CardNo, WORD axis, WORD *MasterType, WORD *MasterIndex, long *MasterEven,
                                           long *SlaveEven, DWORD *MasterSlope);
DMC_API short __stdcall dmc_GearMove(WORD CardNo, WORD AxisNum, WORD *AxisList);

/************************回零运动*************************/
DMC_API short __stdcall dmc_set_home_pin_logic(WORD CardNo, WORD axis, WORD org_logic, double filter);   //设置HOME信号
DMC_API short __stdcall dmc_get_home_pin_logic(WORD CardNo, WORD axis, WORD *org_logic, double *filter); //读取设置HOME信号
DMC_API short __stdcall dmc_set_homemode(WORD CardNo, WORD axis, WORD home_dir, double vel, WORD mode,
                                         WORD EZ_count); //设定指定轴的回原点模式
DMC_API short __stdcall dmc_get_homemode(WORD CardNo, WORD axis, WORD *home_dir, double *vel_mode, WORD *home_mode,
                                         WORD *EZ_count);      //读取指定轴的回原点模式
DMC_API short __stdcall dmc_home_move(WORD CardNo, WORD axis); //启动回零
DMC_API short __stdcall dmc_set_home_profile_unit(WORD CardNo, WORD axis, double Low_Vel, double High_Vel, double Tacc,
                                                  double Tdec); //设置回零速度参数
DMC_API short __stdcall dmc_get_home_profile_unit(WORD CardNo, WORD axis, double *Low_Vel, double *High_Vel, double *Tacc,
                                                  double *Tdec);                  //读取回零速度参数
DMC_API short __stdcall dmc_get_home_result(WORD CardNo, WORD axis, WORD *state); //读取回零执行状态
DMC_API short __stdcall dmc_set_home_position_unit(WORD CardNo, WORD axis, WORD enable, double position);
DMC_API short __stdcall dmc_get_home_position_unit(WORD CardNo, WORD axis, WORD *enable, double *position);
DMC_API short __stdcall dmc_set_el_home(WORD CardNo, WORD axis, WORD mode);

/***************************原点锁存******************************/
DMC_API short __stdcall dmc_set_homelatch_mode(WORD CardNo, WORD axis, WORD enable, WORD logic, WORD source);
DMC_API short __stdcall dmc_get_homelatch_mode(WORD CardNo, WORD axis, WORD *enable, WORD *logic, WORD *source);
DMC_API long __stdcall dmc_get_homelatch_flag(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_reset_homelatch_flag(WORD CardNo, WORD axis);
DMC_API long __stdcall dmc_get_homelatch_value(WORD CardNo, WORD axis);
/*****************************EZ锁存********************************/
DMC_API short __stdcall dmc_set_ezlatch_mode(WORD CardNo, WORD axis, WORD enable, WORD logic, WORD source);
DMC_API short __stdcall dmc_get_ezlatch_mode(WORD CardNo, WORD axis, WORD *enable, WORD *logic, WORD *source);
DMC_API long __stdcall dmc_get_ezlatch_flag(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_reset_ezlatch_flag(WORD CardNo, WORD axis);
DMC_API long __stdcall dmc_get_ezlatch_value(WORD CardNo, WORD axis);

/************************手轮运动函数*********************************/
//一对一
DMC_API short __stdcall dmc_set_handwheel_inmode(WORD CardNo, WORD axis, WORD inmode, long multi,
                                                 double vh); //设置输入手轮脉冲信号的工作方式
DMC_API short __stdcall dmc_get_handwheel_inmode(WORD CardNo, WORD axis, WORD *inmode, long *multi,
                                                 double *vh); //读取输入手轮脉冲信号的工作方式
//一对一浮点型倍率
DMC_API short __stdcall dmc_set_handwheel_inmode_decimals(WORD CardNo, WORD axis, WORD inmode, double multi, double vh);
DMC_API short __stdcall dmc_get_handwheel_inmode_decimals(WORD CardNo, WORD axis, WORD *inmode, double *multi, double *vh);
DMC_API short __stdcall dmc_handwheel_move(WORD CardNo, WORD axis);
//设置手轮通道
DMC_API short __stdcall dmc_set_handwheel_channel(WORD CardNo, WORD index);
DMC_API short __stdcall dmc_get_handwheel_channel(WORD CardNo, WORD *index);
//一对多
DMC_API short __stdcall dmc_set_handwheel_inmode_extern(WORD CardNo, WORD inmode, WORD AxisNum, WORD *AxisList, long *multi);
DMC_API short __stdcall dmc_get_handwheel_inmode_extern(WORD CardNo, WORD *inmode, WORD *AxisNum, WORD *AxisList, long *multi);
//一对多浮点型倍率
DMC_API short __stdcall dmc_set_handwheel_inmode_extern_decimals(WORD CardNo, WORD inmode, WORD AxisNum, WORD *AxisList,
                                                                 double *multi);
DMC_API short __stdcall dmc_get_handwheel_inmode_extern_decimals(WORD CardNo, WORD *inmode, WORD *AxisNum, WORD *AxisList,
                                                                 double *multi);

/*********************************************************************************************************
手轮运动 新增总线的手轮模式  总线
*********************************************************************************************************/
DMC_API short __stdcall dmc_handwheel_set_axislist(WORD CardNo, WORD AxisSelIndex, WORD AxisNum, WORD *AxisList);
DMC_API short __stdcall dmc_handwheel_get_axislist(WORD CardNo, WORD AxisSelIndex, WORD *AxisNum, WORD *AxisList);
DMC_API short __stdcall dmc_handwheel_set_ratiolist(WORD CardNo, WORD AxisSelIndex, WORD StartRatioIndex, WORD RatioSelNum,
                                                    double *RatioList);
DMC_API short __stdcall dmc_handwheel_get_ratiolist(WORD CardNo, WORD AxisSelIndex, WORD StartRatioIndex, WORD RatioSelNum,
                                                    double *RatioList);
DMC_API short __stdcall dmc_handwheel_set_mode(WORD CardNo, WORD InMode, WORD IfHardEnable);
DMC_API short __stdcall dmc_handwheel_get_mode(WORD CardNo, WORD *InMode, WORD *IfHardEnable);
DMC_API short __stdcall dmc_handwheel_set_index(WORD CardNo, WORD AxisSelIndex, WORD RatioSelIndex);
DMC_API short __stdcall dmc_handwheel_get_index(WORD CardNo, WORD *AxisSelIndex, WORD *RatioSelIndex);
DMC_API short __stdcall dmc_handwheel_move(WORD CardNo, WORD ForceMove);
DMC_API short __stdcall dmc_handwheel_stop(WORD CardNo);

/**************************高速锁存函数***************************/
/*************************************
LTC1	AXIS0	AXIS1	AXIS2	AXIS3
LTC2	AXIS4	AXIS5	AXIS6	AXIS7
***************************************/
DMC_API short __stdcall dmc_set_ltc_mode(WORD CardNo, WORD axis, WORD ltc_logic, WORD ltc_mode, double filter); //设置LTC信号
DMC_API short __stdcall dmc_get_ltc_mode(WORD CardNo, WORD axis, WORD *ltc_logic, WORD *ltc_mode,
                                         double *filter); //读取设置LTC信号
DMC_API short __stdcall dmc_set_latch_mode(WORD CardNo, WORD axis, WORD all_enable, WORD latch_source,
                                           WORD triger_chunnel); //设置锁存方式
DMC_API short __stdcall dmc_get_latch_mode(WORD CardNo, WORD axis, WORD *all_enable, WORD *latch_source, WORD *triger_chunnel);
DMC_API short __stdcall dmc_SetLtcOutMode(WORD CardNo, WORD axis, WORD enable, WORD bitno); //反相输出
DMC_API short __stdcall dmc_GetLtcOutMode(WORD CardNo, WORD axis, WORD *enable, WORD *bitno);
DMC_API short __stdcall dmc_get_latch_flag(WORD CardNo, WORD axis);   //读取锁存器标志
DMC_API short __stdcall dmc_reset_latch_flag(WORD CardNo, WORD axis); //复位锁存器标志
DMC_API long __stdcall dmc_get_latch_value(WORD CardNo, WORD axis);   //读取编码器锁存器的值
DMC_API short __stdcall dmc_get_latch_value_unit(WORD CardNo, WORD axis, double *pos_by_mm);
DMC_API short __stdcall dmc_get_latch_flag_extern(WORD CardNo, WORD axis);             //读取锁存器标志
DMC_API long __stdcall dmc_get_latch_value_extern(WORD CardNo, WORD axis, WORD index); //按索引取值
DMC_API short __stdcall dmc_set_latch_stop_time(WORD CardNo, WORD axis, long time);    //触发急停时间
DMC_API short __stdcall dmc_get_latch_stop_time(WORD CardNo, WORD axis, long *time);

/*********************************************************************************************************
高速锁存 新规划20170308 总线
*********************************************************************************************************/
//配置锁存器：锁存模式0-单次锁存，1-连续锁存；锁存边沿0-下降沿，1-上升沿，2-双边沿；滤波时间，单位us
DMC_API short __stdcall dmc_ltc_set_mode(WORD CardNo, WORD latch, WORD ltc_mode, WORD ltc_logic, double filter);
DMC_API short __stdcall dmc_ltc_get_mode(WORD CardNo, WORD latch, WORD *ltc_mode, WORD *ltc_logic, double *filter);
//配置锁存源：0-指令位置，1-编码器反馈位置
DMC_API short __stdcall dmc_ltc_set_source(WORD CardNo, WORD latch, WORD axis, WORD ltc_source);
DMC_API short __stdcall dmc_ltc_get_source(WORD CardNo, WORD latch, WORD axis, WORD *ltc_source);
//复位锁存器
DMC_API short __stdcall dmc_ltc_reset(WORD CardNo, WORD latch);
//读取锁存个数
DMC_API short __stdcall dmc_ltc_get_number(WORD CardNo, WORD latch, WORD axis, int *number);
//读取锁存值
DMC_API short __stdcall dmc_ltc_get_value_unit(WORD CardNo, WORD latch, WORD axis, double *value);

/*****************************位置比较函数****************************/
//单轴位置比较
DMC_API short __stdcall dmc_compare_set_config(WORD CardNo, WORD axis, WORD enable, WORD cmp_source);   //配置比较器
DMC_API short __stdcall dmc_compare_get_config(WORD CardNo, WORD axis, WORD *enable, WORD *cmp_source); //读取配置比较器
DMC_API short __stdcall dmc_compare_clear_points(WORD CardNo, WORD cmp);                                //清除所有比较点
DMC_API short __stdcall dmc_compare_add_point(WORD CardNo, WORD cmp, long pos, WORD dir, WORD action, DWORD actpara); //添加比较点
DMC_API short __stdcall dmc_compare_get_current_point(WORD CardNo, WORD cmp, long *pos);        //读取当前比较点
DMC_API short __stdcall dmc_compare_get_points_runned(WORD CardNo, WORD cmp, long *pointNum);   //查询已经比较过的点
DMC_API short __stdcall dmc_compare_get_points_remained(WORD CardNo, WORD cmp, long *pointNum); //查询可以加入的比较点数量

//二维位置比较
DMC_API short __stdcall dmc_compare_set_config_extern(WORD CardNo, WORD enable, WORD cmp_source);   //配置比较器
DMC_API short __stdcall dmc_compare_get_config_extern(WORD CardNo, WORD *enable, WORD *cmp_source); //读取配置比较器
DMC_API short __stdcall dmc_compare_clear_points_extern(WORD CardNo);                               //清除所有比较点
DMC_API short __stdcall dmc_compare_add_point_extern(WORD CardNo, WORD *axis, long *pos, WORD *dir, WORD action,
                                                     DWORD actpara);                  //添加两轴位置比较点
DMC_API short __stdcall dmc_compare_get_current_point_extern(WORD CardNo, long *pos); //读取当前比较点
DMC_API short __stdcall dmc_compare_add_point_extern_unit(WORD CardNo, WORD *axis, double *pos, WORD *dir, WORD action,
                                                          DWORD actpara);                    //添加两轴位置比较点
DMC_API short __stdcall dmc_compare_get_current_point_extern_unit(WORD CardNo, double *pos); //读取当前比较点
DMC_API short __stdcall dmc_compare_get_points_runned_extern(WORD CardNo, long *pointNum);   //查询已经比较过的点
DMC_API short __stdcall dmc_compare_get_points_remained_extern(WORD CardNo, long *pointNum); //查询可以加入的比较点数量

//多组位置比较
DMC_API short __stdcall dmc_compare_set_config_multi(WORD CardNo, WORD queue, WORD enable, WORD axis,
                                                     WORD cmp_source); //配置比较器
DMC_API short __stdcall dmc_compare_get_config_multi(WORD CardNo, WORD queue, WORD *enable, WORD *axis,
                                                     WORD *cmp_source); //读取配置比较器
DMC_API short __stdcall dmc_compare_add_point_multi(WORD CardNo, WORD cmp, int32 pos, WORD dir, WORD action, DWORD actpara,
                                                    double times); //添加比较点 增强

//高速位置比较
DMC_API short __stdcall dmc_hcmp_set_mode(WORD CardNo, WORD hcmp, WORD cmp_mode); //设置高速比较模式
DMC_API short __stdcall dmc_hcmp_get_mode(WORD CardNo, WORD hcmp, WORD *cmp_mode);
//高速比较模式扩展
DMC_API short __stdcall dmc_hcmp_set_config_extern(WORD CardNo, WORD hcmp, WORD axis, WORD cmp_source, WORD cmp_logic,
                                                   WORD cmp_mode, long dist, long time);
DMC_API short __stdcall dmc_hcmp_get_config_extern(WORD CardNo, WORD hcmp, WORD *axis, WORD *cmp_source, WORD *cmp_logic,
                                                   WORD *cmp_mode, long *dist, long *time);

DMC_API short __stdcall dmc_hcmp_set_config(WORD CardNo, WORD hcmp, WORD axis, WORD cmp_source, WORD cmp_logic,
                                            long time); //设置高速比较参数
DMC_API short __stdcall dmc_hcmp_get_config(WORD CardNo, WORD hcmp, WORD *axis, WORD *cmp_source, WORD *cmp_logic, long *time);
DMC_API short __stdcall dmc_hcmp_add_point(WORD CardNo, WORD hcmp, long cmp_pos);
DMC_API short __stdcall dmc_hcmp_set_liner(WORD CardNo, WORD hcmp, long Increment, long Count); //设置线性模式参数
DMC_API short __stdcall dmc_hcmp_get_liner(WORD CardNo, WORD hcmp, long *Increment, long *Count);
DMC_API short __stdcall dmc_hcmp_get_current_state(WORD CardNo, WORD hcmp, long *remained_points, long *current_point,
                                                   long *runned_points); //读取高速比较状态
DMC_API short __stdcall dmc_hcmp_clear_points(WORD CardNo, WORD hcmp);
DMC_API short __stdcall dmc_read_cmp_pin(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_write_cmp_pin(WORD CardNo, WORD axis, WORD on_off); //控制cmp端口输出

//二维高速位置比较功能
DMC_API short __stdcall dmc_hcmp_2d_set_enable(WORD CardNo, WORD hcmp, WORD cmp_enable);
DMC_API short __stdcall dmc_hcmp_2d_get_enable(WORD CardNo, WORD hcmp, WORD *cmp_enable);
DMC_API short __stdcall dmc_hcmp_2d_set_config(WORD CardNo, WORD hcmp, WORD cmp_mode, WORD x_axis, WORD x_cmp_source, WORD y_axis,
                                               WORD y_cmp_source, long error, WORD cmp_logic, long time, WORD pwm_enable,
                                               double duty, long freq, WORD port_sel, WORD pwm_number);
DMC_API short __stdcall dmc_hcmp_2d_get_config(WORD CardNo, WORD hcmp, WORD *cmp_mode, WORD *x_axis, WORD *x_cmp_source,
                                               WORD *y_axis, WORD *y_cmp_source, long *error, WORD *cmp_logic, long *time,
                                               WORD *pwm_enable, double *duty, long *freq, WORD *port_sel, WORD *pwm_number);
DMC_API short __stdcall dmc_hcmp_2d_add_point(WORD CardNo, WORD hcmp, long x_cmp_pos, long y_cmp_pos);
DMC_API short __stdcall dmc_hcmp_2d_get_current_state(WORD CardNo, WORD hcmp, long *remained_points, long *x_current_point,
                                                      long *y_current_point, long *runned_points, WORD *current_state);
DMC_API short __stdcall dmc_hcmp_2d_clear_points(WORD CardNo, WORD hcmp);
DMC_API short __stdcall dmc_hcmp_2d_force_output(WORD CardNo, WORD hcmp, WORD enable);

/********************通用IO功能**************************/
//通用IO
DMC_API short __stdcall dmc_read_inbit(WORD CardNo, WORD bitno);                        //读取输入口的状态
DMC_API short __stdcall dmc_write_outbit(WORD CardNo, WORD bitno, WORD on_off);         //设置输出口的状态
DMC_API short __stdcall dmc_read_outbit(WORD CardNo, WORD bitno);                       //读取输出口的状态
DMC_API DWORD __stdcall dmc_read_inport(WORD CardNo, WORD portno);                      //读取输入端口的值
DMC_API DWORD __stdcall dmc_read_outport(WORD CardNo, WORD portno);                     //读取输出端口的值
DMC_API short __stdcall dmc_write_outport(WORD CardNo, WORD portno, DWORD outport_val); //设置所有输出端口的值

DMC_API short __stdcall dmc_write_outport_16X(WORD CardNo, WORD portno, DWORD outport_val); //设置通用输出端口的值

//虚拟IO映射
DMC_API short __stdcall dmc_set_io_map_virtual(WORD CardNo, WORD bitno, WORD MapIoType, WORD MapIoIndex, double Filter);
DMC_API short __stdcall dmc_get_io_map_virtual(WORD CardNo, WORD bitno, WORD *MapIoType, WORD *MapIoIndex, double *Filter);
DMC_API short __stdcall dmc_read_inbit_virtual(WORD CardNo, WORD bitno); //读取输入口的状态

DMC_API short __stdcall dmc_reverse_outbit(WORD CardNo, WORD bitno, double reverse_time);         //IO延时翻转
DMC_API short __stdcall dmc_set_io_count_mode(WORD CardNo, WORD bitno, WORD mode, double filter); //设置IO计数模式
DMC_API short __stdcall dmc_get_io_count_mode(WORD CardNo, WORD bitno, WORD *mode, double *filter);
DMC_API short __stdcall dmc_set_io_count_value(WORD CardNo, WORD bitno, DWORD CountValue); //设置IO计数值
DMC_API short __stdcall dmc_get_io_count_value(WORD CardNo, WORD bitno, DWORD *CountValue);

/*********************专用IO信号************************/
DMC_API short __stdcall dmc_set_axis_io_map(WORD CardNo, WORD Axis, WORD IoType, WORD MapIoType, WORD MapIoIndex, double Filter);
DMC_API short __stdcall dmc_get_axis_io_map(WORD CardNo, WORD Axis, WORD IoType, WORD *MapIoType, WORD *MapIoIndex,
                                            double *Filter);
DMC_API short __stdcall dmc_set_special_input_filter(WORD CardNo, double Filter); //设置所有专用IO滤波时间

//3410专用 回原点减速信号配置
DMC_API short __stdcall dmc_set_sd_mode(WORD CardNo, WORD axis, WORD enable, WORD sd_logic, WORD sd_mode);    //设置SD信号
DMC_API short __stdcall dmc_get_sd_mode(WORD CardNo, WORD axis, WORD *enable, WORD *sd_logic, WORD *sd_mode); //读取设置SD信号
//专用IO
DMC_API short __stdcall dmc_set_inp_mode(WORD CardNo, WORD axis, WORD enable, WORD inp_logic);   //设置INP信号
DMC_API short __stdcall dmc_get_inp_mode(WORD CardNo, WORD axis, WORD *enable, WORD *inp_logic); //读取设置INP信号
DMC_API short __stdcall dmc_set_rdy_mode(WORD CardNo, WORD axis, WORD enable, WORD rdy_logic);   //设置RDY信号
DMC_API short __stdcall dmc_get_rdy_mode(WORD CardNo, WORD axis, WORD *enable, WORD *rdy_logic); //读取设置RDY信号
DMC_API short __stdcall dmc_set_erc_mode(WORD CardNo, WORD axis, WORD enable, WORD erc_logic, WORD erc_width,
                                         WORD erc_off_time); //设置ERC信号
DMC_API short __stdcall dmc_get_erc_mode(WORD CardNo, WORD axis, WORD *enable, WORD *erc_logic, WORD *erc_width,
                                         WORD *erc_off_time);                                                   //读取设置ERC信号
DMC_API short __stdcall dmc_set_alm_mode(WORD CardNo, WORD axis, WORD enable, WORD alm_logic, WORD alm_action); //设置ALM信号
DMC_API short __stdcall dmc_get_alm_mode(WORD CardNo, WORD axis, WORD *enable, WORD *alm_logic,
                                         WORD *alm_action);                                                     //读取设置ALM信号
DMC_API short __stdcall dmc_set_ez_mode(WORD CardNo, WORD axis, WORD ez_logic, WORD ez_mode, double filter);    //设置EZ信号
DMC_API short __stdcall dmc_get_ez_mode(WORD CardNo, WORD axis, WORD *ez_logic, WORD *ez_mode, double *filter); //读取设置EZ信号

DMC_API short __stdcall dmc_write_sevon_pin(WORD CardNo, WORD axis, WORD on_off); //输出SEVON信号
DMC_API short __stdcall dmc_read_sevon_pin(WORD CardNo, WORD axis);               //读取SEVON信号
DMC_API short __stdcall dmc_read_rdy_pin(WORD CardNo, WORD axis);                 //读取RDY状态
DMC_API short __stdcall dmc_write_erc_pin(WORD CardNo, WORD axis, WORD on_off);   //控制ERC信号输出
DMC_API short __stdcall dmc_read_erc_pin(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_write_sevrst_pin(WORD CardNo, WORD axis, WORD on_off); //输出伺服复位信号
DMC_API short __stdcall dmc_read_sevrst_pin(WORD CardNo, WORD axis);               //读伺服复位信号

//外部减速停止信号及减速停止时间设置
DMC_API short __stdcall dmc_set_io_dstp_mode(WORD CardNo, WORD axis, WORD enable,
                                             WORD logic); //enable:0-禁用，1-按时间减速停止，2-按距离减速停止
DMC_API short __stdcall dmc_get_io_dstp_mode(WORD CardNo, WORD axis, WORD *enable, WORD *logic);
//减速停止时间
DMC_API short __stdcall dmc_set_dec_stop_time(WORD CardNo, WORD axis, double time);
DMC_API short __stdcall dmc_get_dec_stop_time(WORD CardNo, WORD axis, double *time);
//插补减速停止信号和减速时间设置
DMC_API short __stdcall dmc_set_vector_dec_stop_time(WORD CardNo, WORD Crd, double time);
DMC_API short __stdcall dmc_get_vector_dec_stop_time(WORD CardNo, WORD Crd, double *time);
//减速停止距离
DMC_API short __stdcall dmc_set_dec_stop_dist(WORD CardNo, WORD axis, long dist);
DMC_API short __stdcall dmc_get_dec_stop_dist(WORD CardNo, WORD axis, long *dist);
DMC_API short __stdcall dmc_set_io_dstp_bitno(WORD CardNo, WORD axis, WORD bitno,
                                              double filter); //设置通用输入口的一位减速停止IO口
DMC_API short __stdcall dmc_get_io_dstp_bitno(WORD CardNo, WORD axis, WORD *bitno, double *filter);

/************************编码器函数**********************/
DMC_API short __stdcall dmc_set_counter_inmode(WORD CardNo, WORD axis, WORD mode);  //设定编码器的计数方式
DMC_API short __stdcall dmc_get_counter_inmode(WORD CardNo, WORD axis, WORD *mode); //读取编码器的计数方式
//编码器值(脉冲)
DMC_API long __stdcall dmc_get_encoder(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_set_encoder(WORD CardNo, WORD axis, long encoder_value);
//编码器值(当量)
DMC_API short __stdcall dmc_set_encoder_unit(WORD CardNo, WORD axis, double pos);
DMC_API short __stdcall dmc_get_encoder_unit(WORD CardNo, WORD axis, double *pos);
//手轮编码器 总线卡
DMC_API short __stdcall dmc_set_handwheel_encoder(WORD CardNo, WORD channel, long pos);
DMC_API short __stdcall dmc_get_handwheel_encoder(WORD CardNo, WORD channel, long *pos);
//设置辅助编码模式
DMC_API short __stdcall dmc_set_extra_encoder_mode(WORD CardNo, WORD channel, WORD inmode, WORD multi);
DMC_API short __stdcall dmc_get_extra_encoder_mode(WORD CardNo, WORD channel, WORD *inmode, WORD *multi);
//设置辅助编码器值
DMC_API short __stdcall dmc_set_extra_encoder(WORD CardNo, WORD channel, int pos);
DMC_API short __stdcall dmc_get_extra_encoder(WORD CardNo, WORD channel, int *pos);

/*********************位置计数控制***************************/
//当前位置(脉冲)
DMC_API short __stdcall dmc_set_position(WORD CardNo, WORD axis, long current_position);
DMC_API long __stdcall dmc_get_position(WORD CardNo, WORD axis);
//当前位置(当量)
DMC_API short __stdcall dmc_set_position_unit(WORD CardNo, WORD axis, double pos);
DMC_API short __stdcall dmc_get_position_unit(WORD CardNo, WORD axis, double *pos);

/**************************运动状态********************************/
//轴状态
DMC_API double __stdcall dmc_read_current_speed(WORD CardNo, WORD axis);                            //读取指定轴的当前速度(脉冲)
DMC_API short __stdcall dmc_read_current_speed_unit(WORD CardNo, WORD axis, double *current_speed); //读取当前速度(当量)
DMC_API double __stdcall dmc_read_vector_speed(WORD CardNo);                                        //读取当前卡的插补速度
DMC_API long __stdcall dmc_get_target_position(WORD CardNo, WORD axis);                             //读取指定轴的目标位置
DMC_API short __stdcall dmc_get_target_position_unit(WORD CardNo, WORD axis, double *pos);          //读取指定轴的目标位置(当量)
DMC_API short __stdcall dmc_check_done(WORD CardNo, WORD axis);                                     //读取指定轴的运动状态

DMC_API DWORD __stdcall dmc_axis_io_status(WORD CardNo, WORD axis);                    //读取指定轴有关运动信号的状态
DMC_API short __stdcall dmc_stop(WORD CardNo, WORD axis, WORD stop_mode);              //单轴停止
DMC_API short __stdcall dmc_check_done_multicoor(WORD CardNo, WORD Crd);               //插补运动状态
DMC_API short __stdcall dmc_stop_multicoor(WORD CardNo, WORD Crd, WORD stop_mode);     //停止插补器
DMC_API short __stdcall dmc_emg_stop(WORD CardNo);                                     //紧急停止所有轴
DMC_API short __stdcall dmc_LinkState(WORD CardNo, WORD *State);                       //连接状态
DMC_API short __stdcall dmc_get_axis_run_mode(WORD CardNo, WORD axis, WORD *run_mode); //读取指定轴的运动模式
DMC_API short __stdcall dmc_get_stop_reason(WORD CardNo, WORD axis, long *StopReason); //读取停止原因
DMC_API short __stdcall dmc_clear_stop_reason(WORD CardNo, WORD axis);                 //清除停止原因

//trace功能
DMC_API short __stdcall dmc_set_trace(WORD CardNo, WORD axis, WORD enable);
DMC_API short __stdcall dmc_get_trace(WORD CardNo, WORD axis, WORD *enable);
DMC_API short __stdcall dmc_read_trace_data(WORD CardNo, WORD axis, WORD data_option, long *ReceiveSize, double *time,
                                            double *data, long *remain_num);
DMC_API short __stdcall dmc_trace_start(WORD CardNo, WORD AxisNum, WORD *AxisList);
DMC_API short __stdcall dmc_trace_stop(WORD CardNo);

//弧长计算
DMC_API short __stdcall dmc_calculate_arclength_center(double *start_pos, double *target_pos, double *cen_pos, WORD arc_dir,
                                                       double circle, double *ArcLength);

/*********************************连续插补函数**************************************
连续插补的速度函数和单段插补相同
************************************************************************************/
DMC_API short __stdcall dmc_conti_open_list(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList); //打开连续缓存区
DMC_API short __stdcall dmc_conti_close_list(WORD CardNo, WORD Crd);                              //关闭连续缓存区
DMC_API short __stdcall dmc_conti_reset_list(WORD CardNo, WORD Crd);                              //复位连续缓存区
DMC_API short __stdcall dmc_conti_stop_list(WORD CardNo, WORD Crd, WORD stop_mode);               //连续插补中停止
DMC_API short __stdcall dmc_conti_pause_list(WORD CardNo, WORD Crd);                              //连续插补中暂停
DMC_API short __stdcall dmc_conti_start_list(WORD CardNo, WORD Crd);                              //开始连续插补
DMC_API short __stdcall dmc_conti_get_run_state(WORD CardNo, WORD Crd);    //0-运行，1-暂停，2-正常停止，3-未启动，4-空闲
DMC_API long __stdcall dmc_conti_remain_space(WORD CardNo, WORD Crd);      //查连续插补剩余缓存数
DMC_API long __stdcall dmc_conti_read_current_mark(WORD CardNo, WORD Crd); //读取当前连续插补段的标号

//blend模式
DMC_API short __stdcall dmc_conti_set_blend(WORD CardNo, WORD Crd, WORD enable);
DMC_API short __stdcall dmc_conti_get_blend(WORD CardNo, WORD Crd, WORD *enable);
DMC_API short __stdcall dmc_conti_set_override(WORD CardNo, WORD Crd, double Percent);       //设置插补中速度比例
DMC_API short __stdcall dmc_conti_change_speed_ratio(WORD CardNo, WORD Crd, double percent); //设置插补中动态变速
//小线段前瞻
DMC_API short __stdcall dmc_conti_set_lookahead_mode(WORD CardNo, WORD Crd, WORD enable, long LookaheadSegments, double PathError,
                                                     double LookaheadAcc);
DMC_API short __stdcall dmc_conti_get_lookahead_mode(WORD CardNo, WORD Crd, WORD *enable, long *LookaheadSegments,
                                                     double *PathError, double *LookaheadAcc);

//连续插补IO功能
DMC_API short __stdcall dmc_conti_wait_input(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double TimeOut, long mark);
DMC_API short __stdcall dmc_conti_delay_outbit_to_start(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double delay_value,
                                                        WORD delay_mode, double ReverseTime);
DMC_API short __stdcall dmc_conti_delay_outbit_to_stop(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double delay_time,
                                                       double ReverseTime);
DMC_API short __stdcall dmc_conti_ahead_outbit_to_stop(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double ahead_value,
                                                       WORD ahead_mode, double ReverseTime);
DMC_API short __stdcall dmc_conti_accurate_outbit_unit(WORD CardNo, WORD Crd, WORD cmp_no, WORD on_off, WORD axis, double abs_pos,
                                                       WORD pos_source, double ReverseTime);
DMC_API short __stdcall dmc_conti_write_outbit(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double ReverseTime);
DMC_API short __stdcall dmc_conti_clear_io_action(WORD CardNo, WORD Crd, DWORD Io_Mask);
DMC_API short __stdcall dmc_conti_set_pause_output(WORD CardNo, WORD Crd, WORD action, long mask, long state);
DMC_API short __stdcall dmc_conti_get_pause_output(WORD CardNo, WORD Crd, WORD *action, long *mask, long *state);
DMC_API short __stdcall dmc_conti_delay(WORD CardNo, WORD Crd, double delay_time, long mark); //延时指令

DMC_API short __stdcall dmc_conti_reverse_outbit(WORD CardNo, WORD Crd, WORD bitno, double reverse_time); //IO输出延时翻转
DMC_API short __stdcall dmc_conti_delay_outbit(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double delay_time); //IO延时输出

//连续插补轨迹段
DMC_API short __stdcall dmc_conti_line_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *pPosList, WORD posi_mode,
                                            long mark);
DMC_API short __stdcall dmc_conti_arc_move_center_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                       double *Cen_Pos, WORD Arc_Dir, long Circle, WORD posi_mode, long mark);
DMC_API short __stdcall dmc_conti_arc_move_radius_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                       double Arc_Radius, WORD Arc_Dir, long Circle, WORD posi_mode, long mark);
DMC_API short __stdcall dmc_conti_arc_move_3points_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                        double *Mid_Pos, long Circle, WORD posi_mode, long mark);
DMC_API short __stdcall dmc_conti_rectangle_move_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                      double *Mark_Pos, long num, WORD rect_mode, WORD posi_mode, long mark);
DMC_API short __stdcall dmc_conti_pmove_unit(WORD CardNo, WORD Crd, WORD axis, double dist, WORD posi_mode, WORD mode,
                                             long imark);
DMC_API short __stdcall dmc_conti_set_involute_mode(WORD CardNo, WORD Crd, WORD mode); //设置螺旋线插补运动模式
DMC_API short __stdcall dmc_conti_get_involute_mode(WORD CardNo, WORD Crd, WORD *mode);
DMC_API short __stdcall dmc_set_gear_follow_profile(WORD CardNo, WORD axis, WORD enable, WORD master_axis, double ratio); //双Z轴
DMC_API short __stdcall dmc_get_gear_follow_profile(WORD CardNo, WORD axis, WORD *enable, WORD *master_axis, double *ratio);

DMC_API short __stdcall dmc_conti_line_unit_extern(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                   double *Cen_Pos, WORD posi_mode, long mark);
DMC_API short __stdcall dmc_conti_arc_move_center_unit_extern(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList,
                                                              double *Target_Pos, double *Cen_Pos, double Arc_Radius,
                                                              WORD posi_mode, long mark);

/*********************************PWM功能函数*******************************/
//PWM控制
DMC_API short __stdcall dmc_set_pwm_pin(WORD CardNo, WORD portno, WORD ON_OFF, double dfreqency, double dduty);
DMC_API short __stdcall dmc_get_pwm_pin(WORD CardNo, WORD portno, WORD *ON_OFF, double *dfreqency, double *dduty);
//PWM功能
DMC_API short __stdcall dmc_set_pwm_enable(WORD CardNo, WORD enable);
DMC_API short __stdcall dmc_get_pwm_enable(WORD CardNo, WORD *enable);
DMC_API short __stdcall dmc_set_pwm_output(WORD CardNo, WORD PwmNo, double fDuty, double fFre);
DMC_API short __stdcall dmc_get_pwm_output(WORD CardNo, WORD PwmNo, double *fDuty, double *fFre);
DMC_API short __stdcall dmc_conti_set_pwm_output(WORD CardNo, WORD Crd, WORD PwmNo, double fDuty, double fFre);

//高速PWM功能
DMC_API short __stdcall dmc_set_pwm_enable_extern(WORD CardNo, WORD channel, WORD enable);
DMC_API short __stdcall dmc_get_pwm_enable_extern(WORD CardNo, WORD channel, WORD *enable);

/**********PWM速度跟随**************
mode:跟随模式0-不跟随 保持状态 1-不跟随 输出低电平2-不跟随 输出高电平3-跟随 占空比自动调整4-跟随 频率自动调整
MaxVel:最大运行速度，单位unit
MaxValue:最大输出占空比或者频率
OutValue：设置输出频率或占空比
*************************************/
DMC_API short __stdcall dmc_conti_set_pwm_follow_speed(WORD CardNo, WORD Crd, WORD pwm_no, WORD mode, double MaxVel,
                                                       double MaxValue, double OutValue);
DMC_API short __stdcall dmc_conti_get_pwm_follow_speed(WORD CardNo, WORD Crd, WORD pwm_no, WORD *mode, double *MaxVel,
                                                       double *MaxValue, double *OutValue);
//设置PWM开关对应的占空比
DMC_API short __stdcall dmc_set_pwm_onoff_duty(WORD CardNo, WORD PwmNo, double fOnDuty, double fOffDuty);
DMC_API short __stdcall dmc_get_pwm_onoff_duty(WORD CardNo, WORD PwmNo, double *fOnDuty, double *fOffDuty);
DMC_API short __stdcall dmc_conti_delay_pwm_to_start(WORD CardNo, WORD Crd, WORD pwmno, WORD on_off, double delay_value,
                                                     WORD delay_mode, double ReverseTime);
DMC_API short __stdcall dmc_conti_delay_pwm_to_stop(WORD CardNo, WORD Crd, WORD pwmno, WORD on_off, double delay_time,
                                                    double ReverseTime);
DMC_API short __stdcall dmc_conti_ahead_pwm_to_stop(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double ahead_value,
                                                    WORD ahead_mode, double ReverseTime);
DMC_API short __stdcall dmc_conti_write_pwm(WORD CardNo, WORD Crd, WORD pwmno, WORD on_off, double ReverseTime);

/*********************ADDA输出******************************/
//PWM转DA输出
DMC_API short __stdcall dmc_set_da_enable(WORD CardNo, WORD enable);
DMC_API short __stdcall dmc_get_da_enable(WORD CardNo, WORD *enable);
DMC_API short __stdcall dmc_set_da_output(WORD CardNo, WORD channel, double Vout);
DMC_API short __stdcall dmc_get_da_output(WORD CardNo, WORD channel, double *Vout);
//读取AD输入
DMC_API short __stdcall dmc_get_ad_input(WORD CardNo, WORD channel, double *Vout);
//设置连续插补DA输出
DMC_API short __stdcall dmc_conti_set_da_output(WORD CardNo, WORD Crd, WORD channel, double Vout);
//设置连续DA使能
DMC_API short __stdcall dmc_conti_set_da_enable(WORD CardNo, WORD Crd, WORD enable, WORD channel, long mark);
/**********DA速度跟随**************
da_no:通道号
MaxVel:最大运行速度，单位unit
MaxValue:最大电压
*************************************/
DMC_API short __stdcall dmc_conti_set_da_follow_speed(WORD CardNo, WORD Crd, WORD da_no, double MaxVel, double MaxValue,
                                                      double acc_offset, double dec_offset, double acc_dist, double dec_dist);
DMC_API short __stdcall dmc_conti_get_da_follow_speed(WORD CardNo, WORD Crd, WORD da_no, double *MaxVel, double *MaxValue,
                                                      double *acc_offset, double *dec_offset, double *acc_dist, double *dec_dist);

/******************************CAN IO***********************************/
//baud:0-1M 1-800 2-500 3-250 4-125 5-100
DMC_API short __stdcall dmc_set_can_state(WORD CardNo, WORD NodeNum, WORD state, WORD baud); //0-断开；1-连接；2-复位后自动连接
DMC_API short __stdcall dmc_get_can_state(WORD CardNo, WORD *NodeNum, WORD *state);          ////0-断开；1-连接；2-异常
DMC_API short __stdcall dmc_get_can_errcode(WORD CardNo, WORD *Errcode);
DMC_API short __stdcall dmc_write_can_outbit(WORD CardNo, WORD Node, WORD bitno, WORD on_off);
DMC_API short __stdcall dmc_read_can_outbit(WORD CardNo, WORD Node, WORD bitno);
DMC_API short __stdcall dmc_read_can_inbit(WORD CardNo, WORD Node, WORD bitno);
DMC_API short __stdcall dmc_write_can_outport(WORD CardNo, WORD Node, WORD PortNo, DWORD outport_val);
DMC_API DWORD __stdcall dmc_read_can_outport(WORD CardNo, WORD Node, WORD PortNo);
DMC_API DWORD __stdcall dmc_read_can_inport(WORD CardNo, WORD Node, WORD PortNo);
//读取CAN通讯错误
DMC_API short __stdcall dmc_get_can_errcode_extern(WORD CardNo, WORD *Errcode, WORD *msg_losed, WORD *emg_msg_num,
                                                   WORD *lostHeartB, WORD *EmgMsg);

DMC_API long __stdcall dmc_set_profile_limit(WORD CardNo, WORD axis, double Max_Vel, double Max_Acc, double EvenTime);
DMC_API long __stdcall dmc_get_profile_limit(WORD CardNo, WORD axis, double *Max_Vel, double *Max_Acc, double *EvenTime);
DMC_API long __stdcall dmc_set_vector_profile_limit(WORD CardNo, WORD Crd, double Max_Vel, double Max_Acc, double EvenTime);
DMC_API long __stdcall dmc_get_vector_profile_limit(WORD CardNo, WORD Crd, double *Max_Vel, double *Max_Acc, double *EvenTime);
//小圆限速使能
DMC_API short __stdcall dmc_set_arc_limit(WORD CardNo, WORD Crd, WORD Enable, double MaxCenAcc, double MaxArcError);
DMC_API short __stdcall dmc_get_arc_limit(WORD CardNo, WORD Crd, WORD *Enable, double *MaxCenAcc, double *MaxArcError);

//DMC_API short __stdcall dmc_get_axis_debug_state(WORD CardNo,WORD axis,struct_DebugPara* pack);

//软锁存功能
//配置锁存器：锁存模式0-单次锁存，1-连续锁存；锁存边沿0-下降沿，1-上升沿，2-双边沿；滤波时间，单位us
DMC_API short __stdcall dmc_softltc_set_mode(WORD CardNo, WORD latch, WORD ltc_enable, WORD ltc_mode, WORD ltc_inbit,
                                             WORD ltc_logic, double filter);
DMC_API short __stdcall dmc_softltc_get_mode(WORD CardNo, WORD latch, WORD *ltc_enable, WORD *ltc_mode, WORD *ltc_inbit,
                                             WORD *ltc_logic, double *filter);
//配置锁存源：0-指令位置，1-编码器反馈位置
DMC_API short __stdcall dmc_softltc_set_source(WORD CardNo, WORD latch, WORD axis, WORD ltc_source);
DMC_API short __stdcall dmc_softltc_get_source(WORD CardNo, WORD latch, WORD axis, WORD *ltc_source);
//复位锁存器
DMC_API short __stdcall dmc_softltc_reset(WORD CardNo, WORD latch);
//读取锁存个数
DMC_API short __stdcall dmc_softltc_get_number(WORD CardNo, WORD latch, WORD axis, int *number);
//读取锁存值
DMC_API short __stdcall dmc_softltc_get_value_unit(WORD CardNo, WORD latch, WORD axis, double *value);

DMC_API short __stdcall dmc_set_IoFilter(WORD CardNo, WORD bitno, double filter);
DMC_API short __stdcall dmc_get_IoFilter(WORD CardNo, WORD bitno, double *filter);

//螺距补偿
DMC_API short __stdcall dmc_set_lsc_index_value(WORD CardNo, WORD axis, WORD IndexID, long IndexValue);
DMC_API short __stdcall dmc_get_lsc_index_value(WORD CardNo, WORD axis, WORD IndexID, long *IndexValue);

DMC_API short __stdcall dmc_set_lsc_config(WORD CardNo, WORD axis, WORD Origin, DWORD Interal, DWORD NegIndex, DWORD PosIndex,
                                           double Ratio);
DMC_API short __stdcall dmc_get_lsc_config(WORD CardNo, WORD axis, WORD *Origin, DWORD *Interal, DWORD *NegIndex, DWORD *PosIndex,
                                           double *Ratio);

//看门狗
DMC_API short __stdcall dmc_set_watchdog(WORD CardNo, WORD enable, DWORD time);
DMC_API short __stdcall dmc_call_watchdog(WORD CardNo);

DMC_API short __stdcall dmc_read_diagnoseData(WORD CardNo);
DMC_API short __stdcall dmc_conti_set_cmd_end(WORD CardNo, WORD Crd, WORD enable);

//区域软限位
DMC_API short __stdcall dmc_set_zone_limit_config(WORD CardNo, WORD *axis, WORD *Source, long x_pos_p, long x_pos_n, long y_pos_p,
                                                  long y_pos_n, WORD action_para);
DMC_API short __stdcall dmc_get_zone_limit_config(WORD CardNo, WORD *axis, WORD *Source, long *x_pos_p, long *x_pos_n,
                                                  long *y_pos_p, long *y_pos_n, WORD *action_para);
DMC_API short __stdcall dmc_set_zone_limit_enable(WORD CardNo, WORD enable);

//轴互锁功能
DMC_API short __stdcall dmc_set_interlock_config(WORD CardNo, WORD *axis, WORD *Source, long delta_pos, WORD action_para);
DMC_API short __stdcall dmc_get_interlock_config(WORD CardNo, WORD *axis, WORD *Source, long *delta_pos, WORD *action_para);
DMC_API short __stdcall dmc_set_interlock_enable(WORD CardNo, WORD enable);

//龙门模式的误差保护
DMC_API short __stdcall dmc_set_grant_error_protect(WORD CardNo, WORD axis, WORD enable, DWORD dstp_error, DWORD emg_error);
DMC_API short __stdcall dmc_get_grant_error_protect(WORD CardNo, WORD axis, WORD *enable, DWORD *dstp_error, DWORD *emg_error);

DMC_API short __stdcall dmc_set_safety_param(WORD CardNo, WORD axis, WORD enable, long safety_pos);
DMC_API short __stdcall dmc_get_safety_param(WORD CardNo, WORD axis, WORD *enable, long *safety_pos);
DMC_API short __stdcall dmc_get_diagnose_param(WORD CardNo, WORD axis, long *tartet_pos, int *mode, long *pulse_pos,
                                               long *endcoder_pos);

//物件分拣功能
DMC_API short __stdcall dmc_set_camerablow_config(WORD CardNo, WORD camerablow_en, long cameraPos, WORD piece_num,
                                                  long piece_distance, WORD axis_sel, long latch_distance_min);
DMC_API short __stdcall dmc_get_camerablow_config(WORD CardNo, WORD *camerablow_en, long *cameraPos, WORD *piece_num,
                                                  long *piece_distance, WORD *axis_sel, long *latch_distance_min);
DMC_API short __stdcall dmc_clear_camerablow_errorcode(WORD CardNo);
DMC_API short __stdcall dmc_get_camerablow_errorcode(WORD CardNo, WORD *errorcode);

//配置通用输入（0~15）做为轴的限位信号
DMC_API short __stdcall dmc_set_io_limit_config(WORD CardNo, WORD portno, WORD enable, WORD axis_sel, WORD el_mode,
                                                WORD el_logic);
DMC_API short __stdcall dmc_get_io_limit_config(WORD CardNo, WORD portno, WORD *enable, WORD *axis_sel, WORD *el_mode,
                                                WORD *el_logic);

//手轮滤波参数
DMC_API short __stdcall dmc_set_handwheel_filter(WORD CardNo, WORD axis, double filter_factor);
DMC_API short __stdcall dmc_get_handwheel_filter(WORD CardNo, WORD axis, double *filter_factor);

//读取坐标系各轴的当前规划坐标
DMC_API short __stdcall dmc_conti_get_interp_map(WORD CardNo, WORD Crd, WORD *AxisNum, WORD *AxisList, double *pPosList);
//坐标系错误代码
DMC_API short __stdcall dmc_conti_get_crd_errcode(WORD CardNo, WORD Crd, WORD *errcode);


DMC_API short __stdcall dmc_line_unit_follow(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Dist, WORD posi_mode);
DMC_API short __stdcall dmc_conti_line_unit_follow(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *pPosList,
                                                   WORD posi_mode, long mark);

//连续插补缓冲区DA操作
DMC_API short __stdcall dmc_conti_set_da_action(WORD CardNo, WORD Crd, WORD mode, WORD portno, double dvalue);

//读编码器速度
DMC_API short __stdcall dmc_read_encoder_speed(WORD CardNo, WORD Axis, double *current_speed);

DMC_API short __stdcall dmc_axis_follow_line_enable(WORD CardNo, WORD Crd, WORD enable_flag);

//插补轴脉冲补偿
DMC_API short __stdcall dmc_set_interp_compensation(WORD CardNo, WORD axis, double dvalue, double time);
DMC_API short __stdcall dmc_get_interp_compensation(WORD CardNo, WORD axis, double *dvalue, double *time);

//IO精确停止
DMC_API short __stdcall dmc_set_io_exactstop(WORD CardNo, WORD axis, WORD ioNum, WORD *ioList, WORD enable, WORD valid_logic,
                                             WORD action, WORD move_dir);

//读取相对于起点的距离
DMC_API short __stdcall dmc_get_distance_to_start(WORD CardNo, WORD Crd, double *distance_x, double *distance_y, long imark);
//设置标志位 表示是否开始计算相对起点
DMC_API short __stdcall dmc_set_start_distance_flag(WORD CardNo, WORD Crd, WORD flag);

/******************设置回零限位距离**********************
	参  数：
	CardNo:卡号
	Axis：轴号
	N_limit:负限位脉冲数
	P_limit:正限位脉冲数
	返回值：错误代码
*******************************************************************/
DMC_API short __stdcall dmc_set_home_soft_limit(WORD CardNo, WORD Axis, int N_limit, int P_limit);
DMC_API short __stdcall dmc_get_home_soft_limit(WORD CardNo, WORD Axis, int *N_limit, int *P_limit);

/*********************
	指令说明：实现刀向跟随功能，启动某个轴跟随运动，调用此指令时，跟随轴axis会跟随着此指令后面缓冲区的插补指令合轨迹运动，直到设置的脉冲需求完毕。
	指令类型：插补缓冲区指令
	输入参数：CardNo 坐标系号
	axis 跟随轴
	dist 跟随距离(脉冲当量单位)
        follow_mode //运动模式：0-暂停后启动，1-运动中启动
	imark 段号
	输出参数：无
*******************************/
DMC_API short __stdcall dmc_conti_gear_unit(WORD CardNo, WORD Crd, WORD axis, double dist, WORD follow_mode, long imark);

//轨迹拟合使能设置
DMC_API short __stdcall dmc_set_path_fitting_enable(WORD CardNo, WORD Crd, WORD enable);
//螺距补偿功能(新)
DMC_API short __stdcall dmc_enable_leadscrew_comp(WORD CardNo, WORD axis, WORD enable);
DMC_API short __stdcall dmc_set_leadscrew_comp_config(WORD CardNo, WORD axis, WORD n, int startpos, int lenpos, int *pCompPos,
                                                      int *pCompNeg);
//指定轴做定长位移运动 按固定曲线运动
DMC_API short __stdcall dmc_t_pmove_extern(WORD CardNo, WORD axis, double MidPos, double TargetPos, double Min_Vel,
                                           double Max_Vel, double stop_Vel, double acc, double dec, WORD posi_mode);

/*
功能：设置脉冲计数值和编码器反馈值之间差值的报警阀值
输入参数：CardNo 卡号
axis 轴号
error 差值报警报警阀值
输出参数：无
*/
DMC_API short __stdcall dmc_set_pulse_encoder_count_error(WORD CardNo, WORD axis, WORD error);
DMC_API short __stdcall dmc_get_pulse_encoder_count_error(WORD CardNo, WORD axis, WORD *error);
/*
功能：检查脉冲计数值和编码器反馈值之间差值是否超过报警阀值
	输入参数：CardNo 卡号
	axis 轴号
	输出参数;无
	返回参数：0：差值小于报警阀值
	1：差值大于等于报警阀值
*/
DMC_API short __stdcall dmc_check_pulse_encoder_count_error(WORD CardNo, WORD axis, int *pulse_position, int *enc_position);
/*
强行变位扩展
mid_pos: 中间位置
aim_pos:目标位置
posi_mode: 保留参数，默认为绝对值
*/
DMC_API short __stdcall dmc_update_target_position_extern(WORD CardNo, WORD axis, double mid_pos, double aim_pos, double vel,
                                                          WORD posi_mode);

//新物件分拣功能
//固定
DMC_API short __stdcall dmc_sorting_close(WORD CardNo);
DMC_API short __stdcall dmc_sorting_start(WORD CardNo);
DMC_API short __stdcall dmc_sorting_set_init_config(WORD CardNo, WORD cameraCount, int *pCameraPos, WORD *pCamIONo,
                                                    DWORD cameraTime, WORD cameraTrigLevel, WORD blowCount, int *pBlowPos,
                                                    WORD *pBlowIONo, DWORD blowTime, WORD blowTrigLevel, WORD axis, WORD dir,
                                                    WORD checkMode);
DMC_API short __stdcall dmc_sorting_set_camera_trig_count(WORD CardNo, WORD cameraNum, DWORD cameraTrigCnt);
DMC_API short __stdcall dmc_sorting_get_camera_trig_count(WORD CardNo, WORD cameraNum, DWORD *pCameraTrigCnt, WORD count);
DMC_API short __stdcall dmc_sorting_set_blow_trig_count(WORD CardNo, WORD blowNum, DWORD blowTrigCnt);
DMC_API short __stdcall dmc_sorting_get_blow_trig_count(WORD CardNo, WORD blowNum, DWORD *pBlowTrigCnt, WORD count);
DMC_API short __stdcall dmc_sorting_get_camera_config(WORD CardNo, WORD index, int *pos, DWORD *trigTime, WORD *ioNo,
                                                      WORD *trigLevel);
DMC_API short __stdcall dmc_sorting_get_blow_config(WORD CardNo, WORD index, int *pos, DWORD *trigTime, WORD *ioNo,
                                                    WORD *trigLevel);
DMC_API short __stdcall dmc_sorting_get_blow_status(WORD CardNo, DWORD *trigCntAll, WORD *trigMore, WORD *trigLess);
DMC_API short __stdcall dmc_sorting_trig_blow(WORD CardNo, WORD blowNum);
DMC_API short __stdcall dmc_sorting_set_blow_enable(WORD CardNo, WORD blowNum, WORD enable);
DMC_API short __stdcall dmc_sorting_set_piece_config(WORD CardNo, DWORD maxWidth, DWORD minWidth, DWORD minDistance,
                                                     DWORD minTimeIntervel);
DMC_API short __stdcall dmc_sorting_get_piece_status(WORD CardNo, DWORD *pieceFind, DWORD *piecePassCam, DWORD *dist2next,
                                                     DWORD *pieceWidth);
DMC_API short __stdcall dmc_sorting_set_cam_trig_phase(WORD CardNo, WORD blowNo, double coef);
DMC_API short __stdcall dmc_sorting_set_blow_trig_phase(WORD CardNo, WORD blowNo, double coef);

DMC_API short __stdcall dmc_set_sevon_enable(WORD CardNo, WORD axis, WORD on_off);
DMC_API short __stdcall dmc_get_sevon_enable(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_compare_add_point_cycle(WORD CardNo, WORD cmp, long pos, WORD dir, DWORD bitno, DWORD cycle,
                                                    WORD level); //添加比较点

//使能和设置跟踪编码器误差不在范围内时轴的停止模式
DMC_API short __stdcall dmc_set_encoder_count_error_action_config(WORD CardNo, WORD enable, WORD stopmode);
DMC_API short __stdcall dmc_get_encoder_count_error_action_config(WORD CardNo, WORD *enable, WORD *stopmode);

DMC_API short __stdcall dmc_set_home_el_return(WORD CardNo, WORD axis, WORD enable);

//连续编码器da跟随
DMC_API short __stdcall dmc_conti_set_encoder_da_follow_enable(WORD CardNo, WORD Crd, WORD axis, WORD enable);
DMC_API short __stdcall dmc_conti_get_encoder_da_follow_enable(WORD CardNo, WORD Crd, WORD *axis, WORD *enable);

DMC_API short __stdcall dmc_check_done_pos(WORD CardNo, WORD axis, WORD posi_mode);
DMC_API short __stdcall dmc_set_factor_error(WORD CardNo, WORD axis, double factor, long error);
DMC_API short __stdcall dmc_set_factor(WORD CardNo, WORD axis, double factor);
DMC_API short __stdcall dmc_set_error(WORD CardNo, WORD axis, long error);
DMC_API short __stdcall dmc_get_factor_error(WORD CardNo, WORD axis, double *factor, long *error);
DMC_API short __stdcall dmc_check_success_pulse(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_check_success_encoder(WORD CardNo, WORD axis);

//IO及编码器计数功能
DMC_API short __stdcall dmc_set_io_count_profile(WORD CardNo, WORD chan, WORD bitno, WORD mode, double filter, double count_value,
                                                 WORD *axis_list, WORD axis_num, WORD stop_mode);
DMC_API short __stdcall dmc_get_io_count_profile(WORD CardNo, WORD chan, WORD *bitno, WORD *mode, double *filter,
                                                 double *count_value, WORD *axis_list, WORD *axis_num, WORD *stop_mode);
DMC_API short __stdcall dmc_set_io_count_enable(WORD CardNo, WORD chan, WORD ifenable);
DMC_API short __stdcall dmc_clear_io_count(WORD CardNo, WORD chan);
DMC_API short __stdcall dmc_get_io_count_value_extern(WORD CardNo, WORD chan, long *current_value);

//螺距补偿前的脉冲位置，编码器位置//20191025
DMC_API short __stdcall dmc_get_position_ex(WORD CardNo, WORD axis, double *pos);
DMC_API short __stdcall dmc_get_encoder_ex(WORD CardNo, WORD axis, double *pos);
//螺距补偿前的脉冲位置，编码器位置 当量
DMC_API short __stdcall dmc_get_position_ex_unit(WORD CardNo, WORD axis, double *pos);
DMC_API short __stdcall dmc_get_encoder_ex_unit(WORD CardNo, WORD axis, double *pos);

//回零偏移模式函数
DMC_API short __stdcall dmc_set_home_shift_param(WORD CardNo, WORD axis, WORD pos_clear_mode, double ShiftValue);
DMC_API short __stdcall dmc_get_home_shift_param(WORD CardNo, WORD axis, WORD *pos_clear_mode, double *ShiftValue);

DMC_API short __stdcall dmc_change_speed_extend(WORD CardNo, WORD axis, double Curr_Vel, double Taccdec, WORD pin_num,
                                                WORD trig_mode);

DMC_API short __stdcall dmc_follow_vector_speed_move(WORD CardNo, WORD axis, WORD Follow_AxisNum, WORD *Follow_AxisList,
                                                     double ratio);
DMC_API short __stdcall dmc_conti_line_unit_extend(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *pPosList,
                                                   WORD posi_mode, double Extend_Len, WORD enable, long mark); //连续插补直线
DMC_API short __stdcall dmc_hcmp_2d_set_config_unit(WORD CardNo, WORD hcmp, WORD cmp_mode, WORD x_axis, WORD x_cmp_source,
                                                    double x_cmp_error, WORD y_axis, WORD y_cmp_source, double y_cmp_error,
                                                    WORD cmp_logic, int time);
DMC_API short __stdcall dmc_hcmp_2d_get_config_unit(WORD CardNo, WORD hcmp, WORD *cmp_mode, WORD *x_axis, WORD *x_cmp_source,
                                                    double *x_cmp_error, WORD *y_axis, WORD *y_cmp_source, double *y_cmp_error,
                                                    WORD *cmp_logic, int *time);

DMC_API short __stdcall dmc_hcmp_2d_set_pwmoutput(WORD CardNo, WORD hcmp, WORD pwm_enable, double duty, double freq,
                                                  WORD pwm_number);
DMC_API short __stdcall dmc_hcmp_2d_get_pwmoutput(WORD CardNo, WORD hcmp, WORD *pwm_enable, double *duty, double *freq,
                                                  WORD *pwm_number);
DMC_API short __stdcall dmc_hcmp_2d_add_point_unit(WORD ConnectNo, WORD hcmp, double x_cmp_pos, double y_cmp_pos,
                                                   WORD cmp_outbit);
DMC_API short __stdcall dmc_hcmp_2d_get_current_state_unit(WORD CardNo, WORD hcmp, int *remained_points, double *x_current_point,
                                                           double *y_current_point, int *runned_points, WORD *current_state,
                                                           WORD *current_outbit);

DMC_API short __stdcall dmc_set_home_position(WORD CardNo, WORD axis, WORD enable, double position);
DMC_API short __stdcall dmc_get_home_position(WORD CardNo, WORD axis, WORD *enable, double *position);
DMC_API short __stdcall dmc_conti_line_io_union(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *pPosList,
                                                WORD posi_mode, WORD bitno, WORD on_off, double io_value, WORD io_mode,
                                                WORD MapAxis, WORD pos_source, double ReverseTime, long mark);
//设置编码器方向
DMC_API short __stdcall dmc_set_encoder_dir(WORD CardNo, WORD axis, WORD dir);

//圆弧区域软限位
DMC_API short __stdcall dmc_set_arc_zone_limit_config(WORD CardNo, WORD *AxisList, WORD AxisNum, double *Center, double Radius,
                                                      WORD Source, WORD StopMode);
DMC_API short __stdcall dmc_get_arc_zone_limit_config(WORD CardNo, WORD *AxisList, WORD *AxisNum, double *Center, double *Radius,
                                                      WORD *Source, WORD *StopMode);
DMC_API short __stdcall dmc_get_arc_zone_limit_axis_status(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_set_arc_zone_limit_enable(WORD CardNo, WORD enable);
DMC_API short __stdcall dmc_get_arc_zone_limit_enable(WORD CardNo, WORD *enable);

//1、	启用缓存方式添加比较位置：
DMC_API short __stdcall dmc_hcmp_fifo_set_mode(WORD CardNo, WORD hcmp, WORD fifo_mode);
DMC_API short __stdcall dmc_hcmp_fifo_get_mode(WORD CardNo, WORD hcmp, WORD *fifo_mode);
//2、	读取剩余缓存状态，上位机通过此函数判断是否继续添加比较位置
DMC_API short __stdcall dmc_hcmp_fifo_get_state(WORD CardNo, WORD hcmp, long *remained_points);
//3、	按数组的方式批量添加比较位置
DMC_API short __stdcall dmc_hcmp_fifo_add_point_unit(WORD CardNo, WORD hcmp, WORD num, double *cmp_pos);
//4、	清除比较位置,也会把FPGA的位置同步清除掉
DMC_API short __stdcall dmc_hcmp_fifo_clear_points(WORD CardNo, WORD hcmp);
//添加大数据，会堵塞一段时间，指导数据添加完成
DMC_API short __stdcall dmc_hcmp_fifo_add_table(WORD CardNo, WORD hcmp, WORD num, double *cmp_pos);

//二维高速位置比较缓存
//1、	启用缓存方式添加比较位置：
DMC_API short __stdcall dmc_hcmp_2d_fifo_set_mode(WORD CardNo, WORD hcmp, WORD fifo_mode);
DMC_API short __stdcall dmc_hcmp_2d_fifo_get_mode(WORD CardNo, WORD hcmp, WORD *fifo_mode);
//2、	读取剩余缓存状态，上位机通过此函数判断是否继续添加比较位置
DMC_API short __stdcall dmc_hcmp_2d_fifo_get_state(WORD CardNo, WORD hcmp, long *remained_points);
//3、	按数组的方式批量添加比较位置
DMC_API short __stdcall dmc_hcmp_2d_fifo_add_point_unit(WORD CardNo, WORD hcmp, WORD num, double *x_cmp_pos, double *y_cmp_pos,
                                                        WORD *cmp_outbit);
//4、	清除比较位置,也会把FPGA的位置同步清除掉
DMC_API short __stdcall dmc_hcmp_2d_fifo_clear_points(WORD CardNo, WORD hcmp);
//添加大数据，会堵塞一段时间，指导数据添加完成
DMC_API short __stdcall dmc_hcmp_2d_fifo_add_table(WORD CardNo, WORD hcmp, WORD num, double *x_cmp_pos, double *y_cmp_pos);
//添加大数据，会堵塞一段时间，指导数据添加完成,带输出口
DMC_API short __stdcall dmc_hcmp_2d_fifo_add_table_unit(WORD CardNo, WORD hcmp, WORD num, double *x_cmp_pos, double *y_cmp_pos,
                                                        WORD *cmp_outbit);

//控制卡接线盒断线后是否初始化输出电平
DMC_API short __stdcall dmc_set_output_status_repower(WORD CardNo, WORD enable);

DMC_API short __stdcall dmc_t_pmove_extern_softlanding(WORD CardNo, WORD axis, double MidPos, double TargetPos, double start_Vel,
                                                       double Max_Vel, double stop_Vel, DWORD delay_ms, double Max_Vel2,
                                                       double stop_vel2, double acc_time, double dec_time, WORD posi_mode);
DMC_API short __stdcall dmc_compare_add_point_XD(WORD CardNo, WORD cmp, long pos, WORD dir, WORD action, DWORD actpara,
                                                 long startPos); //硒电定制比较函数

DMC_API short __stdcall dmc_pmove_change_pos_speed_config(WORD CardNo, WORD axis, double tar_vel, double tar_rel_pos,
                                                          WORD trig_mode, WORD source);
DMC_API short __stdcall dmc_get_pmove_change_pos_speed_config(WORD CardNo, WORD axis, double *tar_vel, double *tar_rel_pos,
                                                              WORD *trig_mode, WORD *source);
DMC_API short __stdcall dmc_pmove_change_pos_speed_enable(WORD CardNo, WORD axis, WORD enable);
DMC_API short __stdcall dmc_get_pmove_change_pos_speed_enable(WORD CardNo, WORD axis, WORD *enable);
DMC_API short __stdcall dmc_compare_add_point_extend(WORD CardNo, WORD axis, long pos, WORD dir, WORD action, WORD para_num,
                                                     DWORD *actpara, DWORD compare_time);
DMC_API short __stdcall dmc_get_cmd_position(WORD CardNo, WORD axis, double *pos);
//逻辑采样配置
DMC_API short __stdcall dmc_set_logic_analyzer_config(WORD CardNo, WORD channel, DWORD SampleFre, DWORD SampleDepth,
                                                      WORD SampleMode);
DMC_API short __stdcall dmc_start_logic_analyzer(WORD CardNo, WORD channel, WORD enable);
DMC_API short __stdcall dmc_get_logic_analyzer_counter(WORD CardNo, WORD channel, DWORD *counter);
//凯格定制	 20190923修改凯歌定制函数接口
DMC_API short __stdcall dmc_read_inbit_append(WORD CardNo, WORD bitno);                       //读取输入口的状态
DMC_API short __stdcall dmc_write_outbit_append(WORD CardNo, WORD bitno, WORD on_off);        //设置输出口的状态
DMC_API short __stdcall dmc_read_outbit_append(WORD CardNo, WORD bitno);                      //读取输出口的状态
DMC_API DWORD __stdcall dmc_read_inport_append(WORD CardNo, WORD portno);                     //读取输入端口的值
DMC_API DWORD __stdcall dmc_read_outport_append(WORD CardNo, WORD portno);                    //读取输出端口的值
DMC_API short __stdcall dmc_write_outport_append(WORD CardNo, WORD portno, DWORD port_value); //设置所有输出端口的值
//门运动
DMC_API short __stdcall dmc_m_move_unit(WORD CardNo, WORD Crd, WORD axis_num, WORD *axis_list, double *mid_pos,
                                        double *target_pos, double *saftpos, WORD pos_mode);
DMC_API short __stdcall dmc_get_m_move_config(WORD CardNo, WORD Crd, WORD *axis_num, WORD *axis_list, double *mid_pos,
                                              double *target_pos, double *saftpos, WORD *pos_mode);
// 设置坐标系切向跟随
DMC_API short __stdcall dmc_set_tangent_follow(WORD CardNo, WORD Crd, WORD axis, WORD follow_curve, WORD rotate_dir,
                                               double degree_equivalent);
// 获取指定坐标系切向跟随参数
DMC_API short __stdcall dmc_get_tangent_follow_param(WORD CardNo, WORD Crd, WORD *axis, WORD *follow_curve, WORD *rotate_dir,
                                                     double *degree_equivalent);
// 取消坐标系跟随
DMC_API short __stdcall dmc_disable_follow_move(WORD CardNo, WORD Crd);
// 椭圆插补
DMC_API short __stdcall dmc_ellipse_move(WORD CardNo, WORD Crd, WORD axisNum, WORD *Axis_List, double *Target_Pos,
                                         double *Cen_Pos, double A_Axis_Len, double B_Axis_Len, WORD Dir, WORD Pos_Mode);
DMC_API short __stdcall dmc_read_vector_speed_unit(WORD CardNo, WORD Crd, double *current_speed); //读取当前卡的插补速度
//读取参数遇限位反找使能
DMC_API short __stdcall dmc_get_home_el_return(WORD CardNo, WORD axis, WORD *enable);

//新看门狗功能
DMC_API short __stdcall dmc_set_watchdog_action_event(WORD CardNo, WORD event_mask);
DMC_API short __stdcall dmc_get_watchdog_action_event(WORD CardNo, WORD *event_mask);
DMC_API short __stdcall dmc_set_watchdog_enable(WORD CardNo, double timer_period, WORD enable);
DMC_API short __stdcall dmc_get_watchdog_enable(WORD CardNo, double *timer_period, WORD *enable);
DMC_API short __stdcall dmc_reset_watchdog_timer(WORD CardNo);
//io定制功能
DMC_API short __stdcall dmc_set_io_check_control(WORD CardNo, WORD sensor_in_no, WORD check_mode, WORD A_out_no, WORD B_out_no,
                                                 WORD C_out_no, WORD output_mode);
DMC_API short __stdcall dmc_get_io_check_control(WORD CardNo, WORD *sensor_in_no, WORD *check_mode, WORD *A_out_no,
                                                 WORD *B_out_no, WORD *C_out_no, WORD *output_mode);
DMC_API short __stdcall dmc_stop_io_check_control(WORD CardNo);

//设置限位反找偏移距离
DMC_API short __stdcall dmc_set_el_ret_deviation(WORD CardNo, WORD axis, WORD enable, double deviation);
DMC_API short __stdcall dmc_get_el_ret_deviation(WORD CardNo, WORD axis, WORD *enable, double *deviation);

/*****************************总线相关函数**********************************/
//回零运动
DMC_API short __stdcall nmc_set_home_profile(WORD CardNo, WORD axis, WORD home_mode, double Low_Vel, double High_Vel, double Tacc,
                                             double Tdec, double offsetpos); //设置回零参数，合并函数
DMC_API short __stdcall nmc_get_home_profile(WORD CardNo, WORD axis, WORD *home_mode, double *Low_Vel, double *High_Vel,
                                             double *Tacc, double *Tdec, double *offsetpos);
DMC_API short __stdcall nmc_home_move(WORD CardNo, WORD axis);

//-------------------------总线配置-----------------------
/*******************************************************
portnum表示端口号，定义如下
0: 表示canopen的0号端口
1: 表示canopen的1号端口
10:表示EtherCAT的0号端口
11:表示EtherCAT的1号端口
********************************************************/
DMC_API short __stdcall nmc_set_manager_para(WORD CardNo, WORD PortNum, DWORD Baudrate, WORD ManagerID);
DMC_API short __stdcall nmc_get_manager_para(WORD CardNo, WORD PortNum, DWORD *Baudrate, WORD *ManagerID);
DMC_API short __stdcall nmc_set_manager_od(WORD CardNo, WORD PortNum, WORD Index, WORD SubIndex, WORD ValLength, DWORD Value);
DMC_API short __stdcall nmc_get_manager_od(WORD CardNo, WORD PortNum, WORD Index, WORD SubIndex, WORD ValLength, DWORD *Value);

DMC_API short __stdcall nmc_set_node_od(WORD CardNo, WORD PortNum, WORD NodeNum, WORD Index, WORD SubIndex, WORD ValLength,
                                        long Value);
DMC_API short __stdcall nmc_get_node_od(WORD CardNo, WORD PortNum, WORD NodeNum, WORD Index, WORD SubIndex, WORD ValLength,
                                        long *Value);

DMC_API short __stdcall nmc_set_node_od_pbyte(WORD CardNo, WORD PortNum, WORD NodeNum, WORD Index, WORD SubIndex, WORD Bytes,
                                              unsigned char *Value);
DMC_API short __stdcall nmc_get_node_od_pbyte(WORD CardNo, WORD PortNum, WORD NodeNum, WORD Index, WORD SubIndex, WORD Bytes,
                                              unsigned char *Value);

DMC_API short __stdcall nmc_upload_configfile(WORD CardNo, WORD PortNum, const char *FileName);
DMC_API short __stdcall nmc_reset_to_factory(WORD CardNo, WORD PortNum, WORD NodeNum);
DMC_API short __stdcall nmc_write_to_pci(WORD CardNo, WORD PortNum, WORD NodeNum);
DMC_API short __stdcall nmc_download_configfile(WORD CardNo, WORD PortNum, const char *FileName); //总线ENI配置文件
DMC_API short __stdcall nmc_download_mapfile(WORD CardNo, const char *FileName);                  //总线映射文件

//添加单轴使能函数 255表示全使能
DMC_API short __stdcall nmc_set_axis_enable(WORD CardNo, WORD axis);
DMC_API short __stdcall nmc_set_axis_disable(WORD CardNo, WORD axis);

//清除报警信号
DMC_API short __stdcall nmc_set_alarm_clear(WORD CardNo, WORD PortNum, WORD NodeNum);

DMC_API short __stdcall nmc_get_slave_nodes(WORD CardNo, WORD PortNum, WORD BaudRate, WORD *NodeId, WORD *NodeNum);
//获取总线轴数
DMC_API short __stdcall nmc_get_total_axes(WORD CardNo, DWORD *TotalAxis);
//获取总线ADDA输入输出口数
DMC_API short __stdcall nmc_get_total_adcnum(WORD CardNo, WORD *TotalIn, WORD *TotalOut);
//获取总线IO口数
DMC_API short __stdcall nmc_get_total_ionum(WORD CardNo, WORD *TotalIn, WORD *TotalOut);
//清除端口报警
DMC_API short __stdcall nmc_clear_alarm_fieldbus(WORD CardNo, WORD PortNum);
//获取控制器工作模式  1表示ethercat模式，0表示仿真模式
DMC_API short __stdcall nmc_get_controller_workmode(WORD CardNo, WORD *controller_mode);
//设置控制器工作模式  1表示ethercat模式，0表示仿真模式
DMC_API short __stdcall nmc_set_controller_workmode(WORD CardNo, WORD controller_mode);
//设置ethercat总线循环周期(us)
DMC_API short __stdcall nmc_set_cycletime(WORD CardNo, WORD PortNum, DWORD CycleTime);
//获取ethercat总线循环周期(us)
DMC_API short __stdcall nmc_get_cycletime(WORD CardNo, WORD PortNum, DWORD *CycleTime);

//读取总线线程时间消耗参数
DMC_API short __stdcall
dmc_get_perline_time(WORD CardNo, WORD TypeIndex, DWORD *Averagetime, DWORD *Maxtime,
                     uint64 *Cycles); //TypeIndex:0~6  m_Averagetime ; 平均时间 m_Maxtime;最大时间 uint64  m_Cycles;当前时间
DMC_API short __stdcall nmc_set_axis_run_mode(WORD CardNo, WORD axis,
                                              WORD run_mode); //设置轴的运行模式 1为pp模式，6为回零模式，8为csp模式


//获取轴类型
/*
enum fieldbus_type
{
	virtual_type=0,
	ect_type=1,
	can_type=2,
	pulse_type=3, // or local IO 
	unknown=4
};*/
DMC_API short __stdcall nmc_get_axis_type(WORD CardNo, WORD axis, WORD *Axis_Type);
//获取总线时间量，平均时间，最大时间，执行周期数
DMC_API short __stdcall nmc_get_consume_time_fieldbus(WORD CardNo, WORD PortNum, DWORD *Average_time, DWORD *Max_time,
                                                      uint64 *Cycles);
//清除时间量
DMC_API short __stdcall nmc_clear_consume_time_fieldbus(WORD CardNo, WORD PortNum);
//停止ethercat总线,返回0表示成功，其他参数表示不成功
DMC_API short __stdcall nmc_stop_etc(WORD CardNo, WORD *ETCState);

//---------------------------总线轴模式---------------
//获取轴状态字
DMC_API short __stdcall nmc_get_axis_statusword(WORD CardNo, WORD axis, long *statusword);
//获取总线轴控制字
DMC_API short __stdcall nmc_set_axis_contrlword(WORD CardNo, WORD Axis, long Contrlword);
//设置总线轴控制字
DMC_API short __stdcall nmc_get_axis_contrlword(WORD CardNo, WORD Axis, long *Contrlword);
DMC_API short __stdcall nmc_set_axis_contrlmode(WORD CardNo, WORD Axis, long Contrlmode);
DMC_API short __stdcall nmc_get_axis_contrlmode(WORD CardNo, WORD Axis, long *Contrlmode);

// 获取总线端口错误码
DMC_API short __stdcall nmc_get_errcode(WORD CardNo, WORD channel, WORD *Errcode);
// 获取控制卡错误码
DMC_API short __stdcall nmc_get_card_errcode(WORD CardNo, WORD *Errcode);
// 获取总线轴错误码
DMC_API short __stdcall nmc_get_axis_errcode(WORD CardNo, WORD axis, WORD *Errcode);
// 清除总线端口错误码
DMC_API short __stdcall nmc_clear_errcode(WORD CardNo, WORD channel);
// 清除控制卡错误码
DMC_API short __stdcall nmc_clear_card_errcode(WORD CardNo);
// 清除总线轴错误码
DMC_API short __stdcall nmc_clear_axis_errcode(WORD CardNo, WORD iaxis);

DMC_API short __stdcall nmc_get_LostHeartbeat_Nodes(WORD CardNo, WORD PortNum, WORD *NodeID, WORD *NodeNum);
DMC_API short __stdcall nmc_get_EmergeneyMessege_Nodes(WORD CardNo, WORD PortNum, DWORD *NodeMsg, WORD *MsgNum);
DMC_API short __stdcall nmc_SendNmtCommand(WORD CardNo, WORD PortNum, WORD NodeID, WORD NmtCommand);
DMC_API short __stdcall nmc_syn_move(WORD CardNo, WORD AxisNum, WORD *AxisList, long *Position, WORD *PosiMode);
DMC_API short __stdcall nmc_syn_move_unit(WORD CardNo, WORD AxisNum, WORD *AxisList, double *Position, WORD *PosiMode);
//总线多轴同步运动
DMC_API short __stdcall nmc_sync_pmove_unit(WORD CardNo, WORD AxisNum, WORD *AxisList, double *Dist, WORD *PosiMode);
DMC_API short __stdcall nmc_sync_vmove_unit(WORD CardNo, WORD AxisNum, WORD *AxisList, WORD *Dir);

//设置主站参数
DMC_API short __stdcall nmc_set_master_para(WORD CardNo, WORD PortNum, WORD Baudrate, DWORD NodeCnt, WORD MasterId);
//读取主站参数
DMC_API short __stdcall nmc_get_master_para(WORD CardNo, WORD PortNum, WORD *Baudrate, DWORD *NodeCnt, WORD *MasterId);
//设置io输出
DMC_API short __stdcall nmc_write_outbit(WORD CardNo, WORD NoteID, WORD IoBit, WORD IoValue);
//读取io输出
DMC_API short __stdcall nmc_read_outbit(WORD CardNo, WORD NoteID, WORD IoBit, WORD *IoValue);
//读取io输入
DMC_API short __stdcall nmc_read_inbit(WORD CardNo, WORD NoteID, WORD IoBit, WORD *IoValue);
//设置DA参数
DMC_API short __stdcall nmc_set_da_output(WORD CardNo, WORD NoteID, WORD channel, double Value);
//读取DA参数
DMC_API short __stdcall nmc_get_da_output(WORD CardNo, WORD NoteID, WORD channel, double *Value);
//读取AD参数
DMC_API short __stdcall nmc_get_ad_input(WORD CardNo, WORD NoteID, WORD channel, double *Value);
//配置AD模式
DMC_API short __stdcall nmc_set_ad_mode(WORD CardNo, WORD NoteID, WORD channel, WORD mode, DWORD buffer_nums);
DMC_API short __stdcall nmc_get_ad_mode(WORD CardNo, WORD NoteID, WORD channel, WORD *mode, DWORD buffer_nums);
//配置DA模式
DMC_API short __stdcall nmc_set_da_mode(WORD CardNo, WORD NoteID, WORD channel, WORD mode, DWORD buffer_nums);
DMC_API short __stdcall nmc_get_da_mode(WORD CardNo, WORD NoteID, WORD channel, WORD *mode, DWORD buffer_nums);

//参数写入flash
DMC_API short __stdcall nmc_write_to_flash(WORD CardNo, WORD PortNum, WORD NodeNum);

//总线链接
DMC_API short __stdcall nmc_set_connect_state(WORD CardNo, WORD NodeNum, WORD state,
                                              WORD baud);                               //0-断开；1-连接；2-复位后自动连接
DMC_API short __stdcall nmc_get_connect_state(WORD CardNo, WORD *NodeNum, WORD *state); //0-断开；1-连接；2-异常

//设置io输出32位
DMC_API short __stdcall nmc_write_outport(WORD CardNo, WORD NoteID, WORD portno, DWORD outport_val);
//读取io输出32位
DMC_API short __stdcall nmc_read_outport(WORD CardNo, WORD NoteID, WORD portno, DWORD *outport_val);
//读取io输入32位
DMC_API short __stdcall nmc_read_inport(WORD CardNo, WORD NoteID, WORD portno, DWORD *inport_val);

//轴状态机
DMC_API short __stdcall nmc_get_axis_state_machine(WORD CardNo, WORD axis, WORD *Axis_StateMachine);
//获取轴配置控制模式，返回值（6回零模式，8csp模式）
DMC_API short __stdcall nmc_get_axis_setting_contrlmode(WORD CardNo, WORD axis, long *contrlmode);
// 获取从站个数
DMC_API short __stdcall nmc_get_total_slaves(WORD CardNo, WORD PortNum, WORD *TotalSlaves);
// 获取轴的从站信息
DMC_API short __stdcall nmc_get_axis_node_address(WORD CardNo, WORD axis, WORD *SlaveAddr, WORD *Sub_SlaveAddr);
DMC_API short __stdcall nmc_set_axis_io_out(WORD CardNo, WORD axis, DWORD iostate);
DMC_API DWORD __stdcall nmc_get_axis_io_out(WORD CardNo, WORD axis);
DMC_API DWORD __stdcall nmc_get_axis_io_in(WORD CardNo, WORD axis);


/************************************************************
*
*RTEX卡添加函数
*
*
************************************************************/
DMC_API short __stdcall nmc_start_connect(WORD CardNo, WORD chan, WORD *info, WORD *len);
//DMC_API short __stdcall nmc_get_node_info(WORD CardNo,WORD*info,WORD* len);
DMC_API short __stdcall nmc_get_vendor_info(WORD CardNo, WORD axis, char *info, WORD *len);
DMC_API short __stdcall nmc_get_slave_type_info(WORD CardNo, WORD axis, char *info, WORD *len);
DMC_API short __stdcall nmc_get_slave_name_info(WORD CardNo, WORD axis, char *info, WORD *len);
DMC_API short __stdcall nmc_get_slave_version_info(WORD CardNo, WORD axis, char *info, WORD *len);

DMC_API short __stdcall nmc_write_parameter(WORD CardNo, WORD axis, WORD index, WORD subindex, DWORD para_data);
/**************************************************************
*功能说明：RTEX驱动器写EEPROM操作
*
*
**************************************************************/
DMC_API short __stdcall nmc_write_slave_eeprom(WORD CardNo, WORD axis);
/**************************************************************
*index:rtex驱动器的参数分类
*subindex:rtex驱动器在index类别下的参数编号
*para_data:读出的参数值
**************************************************************/
DMC_API short __stdcall nmc_read_parameter(WORD CardNo, WORD axis, WORD index, WORD subindex, DWORD *para_data);
/**************************************************************
*index:rtex驱动器的参数分类
*subindex:rtex驱动器在index类别下的参数编号
*para_data:读出的参数值
**************************************************************/
DMC_API short __stdcall nmc_read_parameter_attributes(WORD CardNo, WORD axis, WORD index, WORD subindex, DWORD *para_data);
DMC_API short __stdcall nmc_set_cmdcycletime(WORD CardNo, WORD PortNum, DWORD cmdtime);
//设置RTEX总线周期比(us)
DMC_API short __stdcall nmc_get_cmdcycletime(WORD CardNo, WORD PortNum, DWORD *cmdtime);
DMC_API short __stdcall nmc_start_log(WORD CardNo, WORD chan, WORD node, WORD Ifenable);
DMC_API short __stdcall nmc_get_log(WORD CardNo, WORD chan, WORD node, DWORD *data);
DMC_API short __stdcall nmc_config_atuo_log(WORD CardNo, WORD ifenable, WORD dir, WORD byte_index, WORD mask, WORD condition,
                                            DWORD counter);

//扩展PDO
DMC_API short __stdcall nmc_write_rxpdo_extra(WORD CardNo, WORD PortNum, WORD address, WORD DataLen, int Value);
DMC_API short __stdcall nmc_read_rxpdo_extra(WORD CardNo, WORD PortNum, WORD address, WORD DataLen, int *Value);
DMC_API short __stdcall nmc_read_txpdo_extra(WORD CardNo, WORD PortNum, WORD address, WORD DataLen, int *Value);
DMC_API short __stdcall nmc_write_rxpdo_extra_uint(WORD CardNo, WORD PortNum, WORD address, WORD DataLen, DWORD Value);
DMC_API short __stdcall nmc_read_rxpdo_extra_uint(WORD CardNo, WORD PortNum, WORD address, WORD DataLen, DWORD *Value);
DMC_API short __stdcall nmc_read_txpdo_extra_uint(WORD CardNo, WORD PortNum, WORD address, WORD DataLen, DWORD *Value);
DMC_API short __stdcall nmc_get_log_state(WORD CardNo, WORD chan, DWORD *state);
DMC_API short __stdcall nmc_driver_reset(WORD CardNo, WORD axis);
DMC_API short __stdcall nmc_set_offset_pos(WORD CardNo, WORD axis, double offset_pos);
DMC_API short __stdcall nmc_get_offset_pos(WORD CardNo, WORD axis, double *offset_pos);
//清除rtex绝对值编码器的多圈值
DMC_API short __stdcall nmc_clear_abs_driver_multi_cycle(WORD CardNo, WORD axis);
//设置io输出32位总线扩展
DMC_API short __stdcall nmc_write_outport_extern(WORD CardNo, WORD Channel, WORD NoteID, WORD portno, DWORD outport_val);
//读取io输出32位总线扩展
DMC_API short __stdcall nmc_read_outport_extern(WORD CardNo, WORD Channel, WORD NoteID, WORD portno, DWORD *outport_val);
//读取io输入32位总线扩展
DMC_API short __stdcall nmc_read_inport_extern(WORD CardNo, WORD Channel, WORD NoteID, WORD portno, DWORD *inport_val);
//设置io输出
DMC_API short __stdcall nmc_write_outbit_extern(WORD CardNo, WORD Channel, WORD NoteID, WORD IoBit, WORD IoValue);
//读取io输出
DMC_API short __stdcall nmc_read_outbit_extern(WORD CardNo, WORD Channel, WORD NoteID, WORD IoBit, WORD *IoValue);
//读取io输入
DMC_API short __stdcall nmc_read_inbit_extern(WORD CardNo, WORD Channel, WORD NoteID, WORD IoBit, WORD *IoValue);
//返回最近错误码
DMC_API short __stdcall nmc_get_current_fieldbus_state_info(WORD CardNo, WORD Channel, WORD *Axis, WORD *ErrorType,
                                                            WORD *SlaveAddr, DWORD *ErrorFieldbusCode);
// 返回历史错误码
DMC_API short __stdcall nmc_get_detail_fieldbus_state_info(WORD CardNo, WORD Channel, DWORD ReadErrorNum, DWORD *TotalNum,
                                                           DWORD *ActualNum, WORD *Axis, WORD *ErrorType, WORD *SlaveAddr,
                                                           DWORD *ErrorFieldbusCode);

//启动采集
DMC_API short __stdcall nmc_start_pdo_trace(WORD CardNo, WORD Channel, WORD SlaveAddr, WORD Index_Num, DWORD Trace_Len,
                                            WORD *Index, WORD *Sub_Index);
//获取采集参数
DMC_API short __stdcall nmc_get_pdo_trace(WORD CardNo, WORD Channel, WORD SlaveAddr, WORD *Index_Num, DWORD *Trace_Len,
                                          WORD *Index, WORD *Sub_Index);
//设置触发采集参数
DMC_API short __stdcall nmc_set_pdo_trace_trig_para(WORD CardNo, WORD Channel, WORD SlaveAddr, WORD Trig_Index,
                                                    WORD Trig_Sub_Index, int Trig_Value, WORD Trig_Mode);
//获取触发采集参数
DMC_API short __stdcall nmc_get_pdo_trace_trig_para(WORD CardNo, WORD Channel, WORD SlaveAddr, WORD *Trig_Index,
                                                    WORD *Trig_Sub_Index, int *Trig_Value, WORD *Trig_Mode);
//采集清除
DMC_API short __stdcall nmc_clear_pdo_trace_data(WORD CardNo, WORD Channel, WORD SlaveAddr);
//采集停止
DMC_API short __stdcall nmc_stop_pdo_trace(WORD CardNo, WORD Channel, WORD SlaveAddr);
//采集数据读取
DMC_API short __stdcall nmc_read_pdo_trace_data(WORD CardNo, WORD Channel, WORD SlaveAddr, DWORD StartAddr, DWORD Readlen,
                                                DWORD *ActReadlen, BYTE *Data);
//已采集个数
DMC_API short __stdcall nmc_get_pdo_trace_num(WORD CardNo, WORD Channel, WORD SlaveAddr, DWORD *Data_num,
                                              DWORD *Size_of_each_bag);
//采集状态
DMC_API short __stdcall nmc_get_pdo_trace_state(WORD CardNo, WORD Channel, WORD SlaveAddr, WORD *Trace_state);
//总线专用
DMC_API short __stdcall nmc_reset_canopen(WORD CardNo);
DMC_API short __stdcall nmc_reset_rtex(WORD CardNo);
DMC_API short __stdcall nmc_reset_etc(WORD CardNo);
//总线错误处理配置
DMC_API short __stdcall nmc_set_fieldbus_error_switch(WORD CardNo, WORD channel, WORD data);
DMC_API short __stdcall nmc_get_fieldbus_error_switch(WORD CardNo, WORD channel, WORD *data);

DMC_API short __stdcall nmc_torque_move(WORD CardNo, WORD axis, int Torque, WORD PosLimitValid, double PosLimitValue,
                                        WORD PosMode);
DMC_API short __stdcall nmc_change_torque(WORD CardNo, WORD axis, int Torque);
//读取转矩大小
DMC_API short __stdcall nmc_get_torque(WORD CardNo, WORD axis, int *Torque);
//modbus函数
DMC_API short __stdcall dmc_modbus_active_COM1(WORD id, const char *COMID, int speed, int bits, int check, int stop);
DMC_API short __stdcall dmc_modbus_active_COM2(WORD id, const char *COMID, int speed, int bits, int check, int stop);
DMC_API short __stdcall dmc_modbus_active_ETH(WORD id, WORD port);

DMC_API short __stdcall dmc_set_modbus_0x(WORD CardNo, WORD start, WORD inum, const char *pdata);
DMC_API short __stdcall dmc_get_modbus_0x(WORD CardNo, WORD start, WORD inum, char *pdata);
DMC_API short __stdcall dmc_set_modbus_4x(WORD CardNo, WORD start, WORD inum, const WORD *pdata);
DMC_API short __stdcall dmc_get_modbus_4x(WORD CardNo, WORD start, WORD inum, WORD *pdata);

DMC_API short __stdcall dmc_set_modbus_4x_float(WORD CardNo, WORD start, WORD inum, const float *pdata);
DMC_API short __stdcall dmc_get_modbus_4x_float(WORD CardNo, WORD start, WORD inum, float *pdata);
DMC_API short __stdcall dmc_set_modbus_4x_int(WORD CardNo, WORD start, WORD inum, const int *pdata);
DMC_API short __stdcall dmc_get_modbus_4x_int(WORD CardNo, WORD start, WORD inum, int *pdata);

DMC_API short __stdcall dmc_set_grant_error_protect_unit(WORD CardNo, WORD axis, WORD enable, double dstp_error,
                                                         double emg_error);
DMC_API short __stdcall dmc_get_grant_error_protect_unit(WORD CardNo, WORD axis, WORD *enable, double *dstp_error,
                                                         double *emg_error);
//螺距补偿相关
DMC_API short __stdcall dmc_get_leadscrew_comp_config(WORD CardNo, WORD axis, WORD *n, int *startpos, int *lenpos, int *pCompPos,
                                                      int *pCompNeg);
DMC_API short __stdcall dmc_set_leadscrew_comp_config_unit(WORD CardNo, WORD axis, WORD n, double startpos, double lenpos,
                                                           double *pCompPos, double *pCompNeg);
DMC_API short __stdcall dmc_get_leadscrew_comp_config_unit(WORD CardNo, WORD axis, WORD *n, double *startpos, double *lenpos,
                                                           double *pCompPos, double *pCompNeg);
//EZ锁存 原点锁存，软锁存相关
DMC_API short __stdcall dmc_get_homelatch_value_unit(WORD CardNo, WORD axis, double *pos);
DMC_API short __stdcall dmc_get_ezlatch_value_unit(WORD CardNo, WORD axis, double *pos);
//高速锁存
DMC_API short __stdcall dmc_get_latch_value_extern_unit(WORD CardNo, WORD axis, WORD index,
                                                        double *pos_by_mm); //按索引取值读取 未完成
//一维比较
DMC_API short __stdcall dmc_compare_add_point_unit(WORD CardNo, WORD cmp, double pos, WORD dir, WORD action,
                                                   DWORD actpara);                              //添加比较点
DMC_API short __stdcall dmc_compare_get_current_point_unit(WORD CardNo, WORD cmp, double *pos); //读取当前比较点
//多组位置比较
DMC_API short __stdcall dmc_compare_add_point_multi_unit(WORD CardNo, WORD cmp, double pos, WORD dir, WORD action, DWORD actpara,
                                                         double times); //添加比较点 增强
//高速位置比较
DMC_API short __stdcall dmc_hcmp_add_point_unit(WORD CardNo, WORD hcmp, double cmp_pos);
DMC_API short __stdcall dmc_hcmp_set_liner_unit(WORD CardNo, WORD hcmp, double Increment, long Count); //设置线性模式参数
DMC_API short __stdcall dmc_hcmp_get_liner_unit(WORD CardNo, WORD hcmp, double *Increment, long *Count);
DMC_API short __stdcall dmc_hcmp_get_current_state_unit(WORD CardNo, WORD hcmp, long *remained_points, double *current_point,
                                                        long *runned_points); //读取高速比较状态
DMC_API short __stdcall dmc_set_softlimit_unit(WORD CardNo, WORD axis, WORD enable, WORD source_sel, WORD SL_action,
                                               double N_limit, double P_limit); //设置软限位参数
DMC_API short __stdcall dmc_get_softlimit_unit(WORD CardNo, WORD axis, WORD *enable, WORD *source_sel, WORD *SL_action,
                                               double *N_limit, double *P_limit); //读取软限位参数

//两轴位置叠加，高速比较功能
DMC_API short __stdcall dmc_hcmp_set_config_overlap(WORD CardNo, WORD hcmp, WORD axis, WORD cmp_source, WORD cmp_logic, long time,
                                                    WORD axis_num, WORD aux_axis, WORD aux_source);
DMC_API short __stdcall dmc_hcmp_get_config_overlap(WORD CardNo, WORD hcmp, WORD *axis, WORD *cmp_source, WORD *cmp_logic,
                                                    long *time, WORD *axis_num, WORD *aux_axis, WORD *aux_source);

//新版门运动
DMC_API short __stdcall dmc_m_move_set_coodinate(WORD card, WORD liner, WORD axis_num, WORD *axis_list, uint32 in_io_num,
                                                 WORD trig_flag, WORD pos_mode);
DMC_API short __stdcall dmc_m_move_get_coodinate_para(WORD card, WORD liner, WORD *axis_num, WORD *axis_list, uint32 *in_io_num,
                                                      WORD *trig_flag, WORD *pos_mode);
DMC_API short __stdcall dmc_m_move_set_z_para(WORD card, WORD liner, double z_up_safe_pos, double z_up_target_pos,
                                              double z_down_safe_pos, double z_down_target_pos);
DMC_API short __stdcall dmc_m_move_get_z_para(WORD card, WORD liner, double *z_up_safe_pos, double *z_up_target_pos,
                                              double *z_down_safe_pos, double *z_down_target_pos);
DMC_API short __stdcall dmc_m_move_set_xy_para(WORD card, WORD liner, double x_first_safe_pos, double x_second_safe_pos,
                                               double x_target_pos, WORD y_num, double *y_target_pos);
DMC_API short __stdcall dmc_m_move_get_xy_para(WORD card, WORD liner, double *x_first_safe_pos, double *x_second_safe_pos,
                                               double *x_target_pos, WORD y_num, double *y_target_pos);
DMC_API short __stdcall dmc_m_move_axes(WORD card, WORD liner);
DMC_API short __stdcall dmc_m_move_get_run_phase(WORD card, WORD liner, WORD *run_phase);
DMC_API short __stdcall dmc_m_move_stop(WORD card, WORD liner, WORD stop_mode);
DMC_API short __stdcall dmc_t_pmove_extern_unit(WORD CardNo, WORD axis, double MidPos, double TargetPos, double Min_Vel,
                                                double Max_Vel, double stop_Vel, double acc, double dec, WORD posi_mode);

DMC_API short __stdcall dmc_rtcp_set_kinematic_type(WORD CardNo, WORD Crd, WORD machine_type);
DMC_API short __stdcall dmc_rtcp_get_kinematic_type(WORD CardNo, WORD Crd, WORD *machine_type);
//启动或者关闭RTCP功能
DMC_API short __stdcall dmc_rtcp_set_enable(WORD CardNo, WORD Crd, WORD enable);
DMC_API short __stdcall dmc_rtcp_get_enable(WORD CardNo, WORD Crd, WORD *enable);
//位置输入数据类型：0-工件坐标系位置，1-机械坐标系位置
DMC_API short __stdcall dmc_rtcp_set_position_type(WORD CardNo, WORD Crd, WORD position_type);
DMC_API short __stdcall dmc_rtcp_get_position_type(WORD CardNo, WORD Crd, WORD *position_type);
//设置A轴的坐标原点相对于前一个坐标系的偏移, xyz的偏移a_offset[3]
DMC_API short __stdcall dmc_rtcp_set_vector_a(WORD CardNo, WORD Crd, double *a_offset);
DMC_API short __stdcall dmc_rtcp_get_vector_a(WORD CardNo, WORD Crd, double *a_offset);
//设置B轴的坐标原点相对于前一个坐标系的偏移, xyz的偏移b_offset[3]
DMC_API short __stdcall dmc_rtcp_set_vector_b(WORD CardNo, WORD Crd, double *b_offset);
DMC_API short __stdcall dmc_rtcp_get_vector_b(WORD CardNo, WORD Crd, double *b_offset);
//设置C轴的坐标原点相对于前一个坐标系的偏移, xyz的偏移c_offset[3]
DMC_API short __stdcall dmc_rtcp_set_vector_c(WORD CardNo, WORD Crd, double *c_offset);
DMC_API short __stdcall dmc_rtcp_get_vector_c(WORD CardNo, WORD Crd, double *c_offset);
//设置A，B，C轴的偏移位置,
//A,B,C在回0后，再移动到初始姿态的位置，这个时候的A/B/C的偏移角度，
//如果到了初始姿态的位置不进行清0，这个时候就要设置偏移角度
DMC_API short __stdcall dmc_rtcp_set_rotate_joint_offset(WORD CardNo, WORD Crd, double A, double B, double C);
DMC_API short __stdcall dmc_rtcp_get_rotate_joint_offset(WORD CardNo, WORD Crd, double *A, double *B, double *C);
//设置各轴的方向与工件坐标系的关系
//轴相对于工件的方向，如果轴正向运动的时候，驱动刀具相对于工件也是正向运动的，设定为1
//否则设为-1，初始设置为1
//dir[5],对应的是X,Y,Z,旋转轴1，旋转轴2
DMC_API short __stdcall dmc_rtcp_set_joints_direction(WORD CardNo, WORD Crd, int *dir);
DMC_API short __stdcall dmc_rtcp_get_joints_direction(WORD CardNo, WORD Crd, int *dir);
//设置刀具长度，在双摆头中有作用
DMC_API short __stdcall dmc_rtcp_set_tool_length(WORD CardNo, WORD Crd, double tool);
DMC_API short __stdcall dmc_rtcp_get_tool_length(WORD CardNo, WORD Crd, double *tool);

DMC_API short __stdcall dmc_rtcp_get_actual_pos(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *actual_pos);
DMC_API short __stdcall dmc_rtcp_get_command_pos(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *command_pos);

DMC_API short __stdcall dmc_rtcp_kinematics_forward(WORD CardNo, WORD Crd, double *joint_pos, double *axis_pos);
DMC_API short __stdcall dmc_rtcp_kinematics_inverse(WORD CardNo, WORD Crd, double *axis_pos, double *joint_pos);
//设置纯旋转轴的速度
DMC_API short __stdcall dmc_rtcp_set_max_rotate_param(WORD CardNo, WORD Crd, double rotate_speed, double rotate_acc);
DMC_API short __stdcall dmc_rtcp_get_max_rotate_param(WORD CardNo, WORD Crd, double *rotate_speed, double *rotate_acc);
//设置工件原点偏置值
DMC_API short __stdcall dmc_rtcp_set_workpiece_offset(WORD CardNo, WORD Crd, WORD workpiece_index, double *offset);
DMC_API short __stdcall dmc_rtcp_get_workpiece_offset(WORD CardNo, WORD Crd, WORD workpiece_index, double *offset);
//设置工件原点偏置方式
DMC_API short __stdcall dmc_rtcp_set_workpiece_mode(WORD CardNo, WORD Crd, WORD enable, WORD workpiece_index);
DMC_API short __stdcall dmc_rtcp_get_workpiece_mode(WORD CardNo, WORD Crd, WORD *enable, WORD *workpiece_index);

//螺旋插补
DMC_API short __stdcall dmc_conti_helix_move_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AixsList, double *StartPos,
                                                  double *TargetPos, WORD Arc_Dir, int Circle, WORD mode, int mark);
DMC_API short __stdcall dmc_helix_move_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *StartPos,
                                            double *TargetPos, WORD Arc_Dir, int Circle, WORD mode);

DMC_API short __stdcall dmc_compare_add_point_cycle_unit(WORD CardNo, WORD cmp, double pos, WORD dir, DWORD bitno, DWORD cycle,
                                                         WORD level); //添加比较点
//PDO缓存20190715
DMC_API short __stdcall dmc_pdo_buffer_enter(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_pdo_buffer_stop(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_pdo_buffer_clear(WORD CardNo, WORD axis);

DMC_API short __stdcall dmc_pdo_buffer_run_state(WORD CardNo, WORD axis, int *RunState, int *Remain, int *NotRunned, int *Runned);
DMC_API short __stdcall dmc_pdo_buffer_add_data(WORD CardNo, WORD axis, int size, int *data_table);

DMC_API short __stdcall dmc_pdo_buffer_start_multi(WORD CardNo, WORD AxisNum, WORD *AxisList, WORD *ResultList);
DMC_API short __stdcall dmc_pdo_buffer_pause_multi(WORD CardNo, WORD AxisNum, WORD *AxisList, WORD *ResultList);
DMC_API short __stdcall dmc_pdo_buffer_stop_multi(WORD CardNo, WORD AxisNum, WORD *AxisList, WORD *ResultList);
DMC_API short __stdcall dmc_pdo_buffer_add_data_multi(WORD CardNo, WORD AxisNum, WORD *AxisList, int size, int **data_table);
DMC_API short __stdcall dmc_calculate_arccenter_3point(double *start_pos, double *mid_pos, double *target_pos, double *cen_pos);

DMC_API short __stdcall dmc_cmd_buf_open(WORD card, WORD group, WORD axis_num, WORD *axis_list);
DMC_API short __stdcall dmc_cmd_buf_close(WORD card, WORD group);
DMC_API short __stdcall dmc_cmd_buf_start(WORD card, WORD group);
DMC_API short __stdcall dmc_cmd_buf_stop(WORD card, WORD group, WORD stop_mode);
DMC_API short __stdcall dmc_cmd_buf_get_group_config(WORD card, WORD group, WORD *enable, WORD *axis_num, WORD *axis_list);
DMC_API short __stdcall dmc_cmd_buf_get_group_run_info(WORD card, WORD group, WORD *enable, DWORD *array_id, DWORD *stop_reason,
                                                       WORD *trig_phase, WORD *phase);
DMC_API short __stdcall dmc_cmd_buf_add_cmd(WORD card, WORD group, DWORD index, WORD cmd_type, WORD ProcessMode, WORD Dim,
                                            WORD *AxisList, double *TargetPositionFirst, double *m_TargetPositionSecond,
                                            WORD *m_SoftlandFlag, double *SoftLandVel, double *SoftLandEndVel, WORD *m_PosMode,
                                            double *m_TrigAheadPos, WORD m_TrigFlag, WORD m_TrigAxisNum, WORD *m_TrigAxislist,
                                            WORD *m_TrigPosType, WORD *m_TrigAxisPosRelationgship, double *m_TrigAxisPos,
                                            WORD m_TrigIONum, WORD *m_TrigIOState, WORD *m_TrigINIOList, DWORD m_DelayCmdTime,
                                            WORD m_IOList, WORD m_IOState, WORD m_TrigError);
DMC_API short __stdcall dmc_cmd_buf_set_axis_profile(WORD card, WORD group, WORD axis_num, WORD *axis_list, double *start_vel,
                                                     double *max_vel, double *stop_vel, double *tacc, double *tdec);

DMC_API short __stdcall dmc_m_set_muti_profile_unit(WORD CardNo, WORD group, WORD axis_num, WORD *axis_list, double *start_vel,
                                                    double *max_vel, double *tacc, double *tdec, double *stop_vel);
DMC_API short __stdcall dmc_m_set_profile_unit(WORD CardNo, WORD group, WORD axis, double start_vel, double max_vel, double tacc,
                                               double tdec, double stop_vel);
DMC_API short __stdcall dmc_m_add_sigaxis_moveseg_data(WORD CardNo, WORD group, WORD axis, double Target_pos, WORD process_mode,
                                                       DWORD mark);
DMC_API short __stdcall dmc_m_add_sigaxis_move_twoseg_data(WORD CardNo, WORD group, WORD axis, double Target_pos,
                                                           double softland_pos, double softland_vel, double softland_endvel,
                                                           WORD process_mode, DWORD mark);
DMC_API short __stdcall dmc_m_add_mutiaxis_moveseg_data(WORD CardNo, WORD group, WORD axisnum, WORD *axis_list,
                                                        double *Target_pos, WORD process_mode, DWORD mark);
DMC_API short __stdcall dmc_m_add_mutiaxis_move_twoseg_data(WORD CardNo, WORD group, WORD axisnum, WORD *axis_list,
                                                            double *Target_pos, double *softland_pos, double *softland_vel,
                                                            double *softland_endvel, WORD process_mode, DWORD mark);
DMC_API short __stdcall dmc_m_add_ioTrig_movseg_data(WORD CardNo, WORD group, WORD axisnum, WORD *axisList, double *Target_pos,
                                                     WORD process_mode, WORD trigINbit, WORD trigINstate,
                                                     DWORD mark); //io触发移动
DMC_API short __stdcall dmc_m_add_mutiposTrig_movseg_data(WORD CardNo, WORD group, WORD axis, double Target_pos,
                                                          WORD process_mode, WORD trigaxisNum, WORD *trigAxisList,
                                                          double *trigPos, WORD *trigPosType, WORD *trigPosMode,
                                                          DWORD mark); //位置触发移动
DMC_API short __stdcall dmc_m_add_mutiposTrig_mov_twoseg_data(WORD CardNo, WORD group, WORD axis, double Target_pos,
                                                              double softland_pos, double softland_vel, double softland_endvel,
                                                              WORD process_mode, WORD trigAxisNum, WORD *trigAxisList,
                                                              double *trigPos, WORD *trigPosType, WORD *trigPosMode,
                                                              DWORD mark); //多轴位置触发移动
DMC_API short __stdcall dmc_m_add_upseg_data(WORD CardNo, WORD group, WORD axis, double Target_pos, DWORD mark);
DMC_API short __stdcall dmc_m_add_up_twoseg_data(WORD CardNo, WORD group, WORD axis, double Target_pos, double softland_pos,
                                                 double softland_vel, double softland_endvel, DWORD mark);
DMC_API short __stdcall dmc_m_add_ioPosTrig_movseg_data(WORD CardNo, WORD group, WORD axisnum, WORD *axisList, double *Target_pos,
                                                        WORD process_mode, WORD trigAxis, double trigPos, WORD trigPosType,
                                                        WORD trigMode, WORD TrigINNum, WORD *trigINList, WORD *trigINstate,
                                                        DWORD mark); //位置+io触发移动
DMC_API short __stdcall dmc_m_add_ioPosTrig_mov_twoseg_data(WORD CardNo, WORD group, WORD axisnum, WORD *axisList,
                                                            double *Target_pos, double *softland_pos, double *softland_vel,
                                                            double *softland_endvel, WORD process_mode, WORD trigAxis,
                                                            double trigPos, WORD trigPosType, WORD trigMode, WORD TrigINNum,
                                                            WORD *trigINList, WORD *trigINstate, DWORD mark); //位置+io触发移动
DMC_API short __stdcall dmc_m_add_posTrig_movseg_data(WORD CardNo, WORD group, WORD axisnum, WORD *axisList, double *Target_pos,
                                                      WORD process_mode, WORD trigAxis, double trigPos, WORD trigPosType,
                                                      WORD trigMode, DWORD mark); //位置触发移动
DMC_API short __stdcall dmc_m_add_posTrig_mov_twoseg_data(WORD CardNo, WORD group, WORD axisnum, WORD *axisList,
                                                          double *Target_pos, double *softland_pos, double *softland_vel,
                                                          double *softland_endvel, WORD process_mode, WORD trigAxis,
                                                          double trigPos, WORD trigPosType, WORD trigMode,
                                                          DWORD mark); //位置触发移动
DMC_API short __stdcall dmc_m_add_ioPosTrig_down_seg_data(WORD CardNo, WORD group, WORD axis, double safePos, double Target_pos,
                                                          WORD trigAxisNum, WORD *trigAxisList, double *trigPos,
                                                          WORD *trigPosType, WORD *trigMode, WORD trigIN, WORD trigINstate,
                                                          DWORD mark);
DMC_API short __stdcall dmc_m_add_ioPosTrig_down_twoseg_data(WORD CardNo, WORD group, WORD axis, double safePos,
                                                             double Target_pos, double softland_pos, double softland_vel,
                                                             double softland_endvel, WORD trigAxisNum, WORD *trigAxisList,
                                                             double *trigPos, WORD *trigPosType, WORD *trigMode, WORD trigIN,
                                                             WORD trigINstate, DWORD mark);
DMC_API short __stdcall dmc_m_add_posTrig_down_seg_data(WORD CardNo, WORD group, WORD axis, double safePos, double Target_pos,
                                                        WORD trigAxisNum, WORD *trigAxisList, double *trigPos, WORD *trigPosType,
                                                        WORD *trigMode, DWORD mark);
DMC_API short __stdcall dmc_m_add_posTrig_down_twoseg_data(WORD CardNo, WORD group, WORD axis, double safePos, double Target_pos,
                                                           double softland_pos, double softland_vel, double softland_endvel,
                                                           WORD trigAxisNum, WORD *trigAxisList, double *trigPos,
                                                           WORD *trigPosType, WORD *trigMode, DWORD mark);
DMC_API short __stdcall dmc_m_add_posTrig_torque_movseg_data(WORD CardNo, WORD group, WORD axis, double torque,
                                                             double PosLimitValue, WORD PosLimitValid, WORD PosMode,
                                                             WORD trigAxis, double trigPos, WORD trigPosType, WORD trigMode,
                                                             DWORD mark); //位置触发转矩移动

DMC_API short __stdcall dmc_m_posTrig_outbit(WORD CardNo, WORD group, WORD bitno, WORD on_off, WORD ahead_axis,
                                             double ahead_value, WORD ahead_PosType, WORD ahead_Mode, DWORD mark);
DMC_API short __stdcall dmc_m_immediate_write_outbit(WORD CardNo, WORD group, WORD bitno, WORD on_off, DWORD mark);
DMC_API short __stdcall dmc_m_wait_input(WORD CardNo, WORD group, WORD bitno, WORD on_off, double time_out, DWORD mark);
DMC_API short __stdcall dmc_m_delay_time(WORD CardNo, WORD group, double delay_time, DWORD mark);
DMC_API short __stdcall dmc_m_get_run_state(WORD CardNo, WORD group, WORD *state, WORD *enable, DWORD *stop_reason,
                                            WORD *trig_phase, DWORD *mark);
DMC_API short __stdcall dmc_m_open_list(WORD CardNo, WORD group, WORD axis_num, WORD *axis_list);
DMC_API short __stdcall dmc_m_close_list(WORD CardNo, WORD group);
DMC_API short __stdcall dmc_m_start_list(WORD CardNo, WORD group);
DMC_API short __stdcall dmc_m_stop_list(WORD CardNo, WORD group, WORD stopMode);
DMC_API short __stdcall dmc_m_add_posTrig_down_seg_cmd_data(WORD CardNo, WORD group, WORD axis, double safePos, double Target_pos,
                                                            WORD trigAxisNum, WORD *trigAxisList, DWORD mark);
DMC_API short __stdcall dmc_m_add_posTrig_down_twoseg_cmd_data(WORD CardNo, WORD group, WORD axis, double safePos,
                                                               double Target_pos, double softland_pos, double softland_vel,
                                                               double softland_endvel, WORD trigAxisNum, WORD *trigAxisList,
                                                               DWORD mark);
DMC_API short __stdcall dmc_m_pause_list(WORD CardNo, WORD group, WORD stop_mode);

DMC_API short __stdcall dmc_get_ad_input_all(WORD CardNo, double *Vout);
DMC_API short __stdcall dmc_conti_pmove_unit_pausemode(WORD CardNo, WORD axis, double TargetPos, double Min_Vel, double Max_Vel,
                                                       double stop_Vel, double acc, double dec, double smooth_time,
                                                       WORD posi_mode);
DMC_API short __stdcall dmc_conti_return_pausemode(WORD CardNo, WORD Crd, WORD axis);
DMC_API short __stdcall dmc_cmd_buf_temp_stop(WORD CardNo, WORD group, WORD stop_mode);
DMC_API short __stdcall dmc_check_if_crc_support(WORD CardNo);
//编码器da跟随
DMC_API short __stdcall dmc_set_encoder_da_follow_enable(WORD CardNo, WORD axis, WORD enable);
DMC_API short __stdcall dmc_get_encoder_da_follow_enable(WORD CardNo, WORD axis, WORD *enable);

//轴碰撞检测功能接口
DMC_API short __stdcall dmc_set_axis_conflict_config(WORD CardNo, WORD *axis_list, WORD *axis_depart_dir, double home_dist,
                                                     double conflict_dist, WORD stop_mode);
DMC_API short __stdcall dmc_get_axis_conflict_config(WORD CardNo, WORD *axis_list, WORD *axis_depart_dir, double *home_dist,
                                                     double *conflict_dist, WORD *stop_mode);
DMC_API short __stdcall dmc_axis_conflict_config_en(WORD CardNo, WORD enable);
DMC_API short __stdcall dmc_get_axis_conflict_config_en(WORD CardNo, WORD *enable);

//trig_num 触发次数，trig_pos 触发位置
DMC_API short __stdcall dmc_get_pmove_change_pos_speed_state(WORD CardNo, WORD axis, WORD *trig_num, double *trig_pos);

//读输入输出增加带返回值的接口
DMC_API short __stdcall dmc_read_inbit_ex(WORD CardNo, WORD bitno, WORD *state);     //读取输入口的状态
DMC_API short __stdcall dmc_read_outbit_ex(WORD CardNo, WORD bitno, WORD *state);    //读取输出口的状态
DMC_API short __stdcall dmc_read_inport_ex(WORD CardNo, WORD portno, DWORD *state);  //读取输入端口的值
DMC_API short __stdcall dmc_read_outport_ex(WORD CardNo, WORD portno, DWORD *state); //读取输出端口的值
//模块增加读取状态
//设置io输出
DMC_API short __stdcall nmc_write_outbit_ex(WORD CardNo, WORD NoteID, WORD IoBit, WORD IoValue, WORD *state);
//读取io输出
DMC_API short __stdcall nmc_read_outbit_ex(WORD CardNo, WORD NoteID, WORD IoBit, WORD *IoValue, WORD *state);
//读取io输入
DMC_API short __stdcall nmc_read_inbit_ex(WORD CardNo, WORD NoteID, WORD IoBit, WORD *IoValue, WORD *state);
//设置io输出32位
DMC_API short __stdcall nmc_write_outport_ex(WORD CardNo, WORD NoteID, WORD portno, DWORD outport_val, WORD *state);
//读取io输出32位
DMC_API short __stdcall nmc_read_outport_ex(WORD CardNo, WORD NoteID, WORD portno, DWORD *outport_val, WORD *state);
//读取io输入32位
DMC_API short __stdcall nmc_read_inport_ex(WORD CardNo, WORD NoteID, WORD portno, DWORD *inport_val, WORD *state);

//设置DA参数
DMC_API short __stdcall nmc_set_da_output_ex(WORD CardNo, WORD NoteID, WORD channel, double Value, WORD *state);
//读取DA参数
DMC_API short __stdcall nmc_get_da_output_ex(WORD CardNo, WORD NoteID, WORD channel, double *Value, WORD *state);
//读取AD参数
DMC_API short __stdcall nmc_get_ad_input_ex(WORD CardNo, WORD NoteID, WORD channel, double *Value, WORD *state);
//配置AD模式
DMC_API short __stdcall nmc_set_ad_mode_ex(WORD CardNo, WORD NoteID, WORD channel, WORD mode, DWORD buffer_nums, WORD *state);
DMC_API short __stdcall nmc_get_ad_mode_ex(WORD CardNo, WORD NoteID, WORD channel, WORD *mode, DWORD buffer_nums, WORD *state);
//配置DA模式
DMC_API short __stdcall nmc_set_da_mode_ex(WORD CardNo, WORD NoteID, WORD channel, WORD mode, DWORD buffer_nums, WORD *state);
DMC_API short __stdcall nmc_get_da_mode_ex(WORD CardNo, WORD NoteID, WORD channel, WORD *mode, DWORD buffer_nums, WORD *state);
//参数写入flash
DMC_API short __stdcall nmc_write_to_flash_ex(WORD CardNo, WORD PortNum, WORD NodeNum, WORD *state);

//物件分拣加通道
DMC_API short __stdcall dmc_sorting_close_ex(WORD CardNo, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_start_ex(WORD CardNo, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_set_init_config_ex(WORD CardNo, WORD cameraCount, int *pCameraPos, WORD *pCamIONo,
                                                       DWORD cameraTime, WORD cameraTrigLevel, WORD blowCount, int *pBlowPos,
                                                       WORD *pBlowIONo, DWORD blowTime, WORD blowTrigLevel, WORD axis, WORD dir,
                                                       WORD checkMode, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_set_camera_trig_count_ex(WORD CardNo, WORD cameraNum, DWORD cameraTrigCnt, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_get_camera_trig_count_ex(WORD CardNo, WORD cameraNum, DWORD *pCameraTrigCnt, WORD count,
                                                             WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_set_blow_trig_count_ex(WORD CardNo, WORD blowNum, DWORD blowTrigCnt, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_get_blow_trig_count_ex(WORD CardNo, WORD blowNum, DWORD *pBlowTrigCnt, WORD count,
                                                           WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_get_camera_config_ex(WORD CardNo, WORD index, int *pos, DWORD *trigTime, WORD *ioNo,
                                                         WORD *trigLevel, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_get_blow_config_ex(WORD CardNo, WORD index, int *pos, DWORD *trigTime, WORD *ioNo,
                                                       WORD *trigLevel, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_get_blow_status_ex(WORD CardNo, DWORD *trigCntAll, WORD *trigMore, WORD *trigLess,
                                                       WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_trig_blow_ex(WORD CardNo, WORD blowNum, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_set_blow_enable_ex(WORD CardNo, WORD blowNum, WORD enable, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_set_piece_config_ex(WORD CardNo, DWORD maxWidth, DWORD minWidth, DWORD minDistance,
                                                        DWORD minTimeIntervel, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_get_piece_status_ex(WORD CardNo, DWORD *pieceFind, DWORD *piecePassCam, DWORD *dist2next,
                                                        DWORD *pieceWidth, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_set_cam_trig_phase_ex(WORD CardNo, WORD blowNo, double coef, WORD sortModuleNo);
DMC_API short __stdcall dmc_sorting_set_blow_trig_phase_ex(WORD CardNo, WORD blowNo, double coef, WORD sortModuleNo);
//获取分拣指令数量函数
DMC_API short __stdcall dmc_get_sortdev_blow_cmd_cnt(WORD CardNo, WORD blowDevNum, long *cnt);
//获取未处理指令数量函数函数
DMC_API short __stdcall dmc_get_sortdev_blow_cmderr_cnt(WORD CardNo, WORD blowDevNum, long *errCnt);
//分拣队列状态
DMC_API short __stdcall dmc_get_sortqueue_status(WORD CardNo, long *curSorQueueLen, long *passCamWithNoCmd);

// 椭圆连续插补
DMC_API short __stdcall dmc_conti_ellipse_move_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                    double *Cen_Pos, double A_Axis_Len, double B_Axis_Len, WORD Dir,
                                                    WORD Pos_Mode, long mark);
//获取轴状态函数
DMC_API short dmc_get_axis_status_advance(WORD CardNo, WORD axis_no, WORD motion_no, WORD *axis_plan_state, DWORD *ErrPlulseCnt,
                                          WORD *fpga_busy);
//连续插补vmove
DMC_API short __stdcall dmc_conti_vmove_unit(WORD CardNo, WORD Crd, WORD axis, double vel, double acc, WORD dir, long imark);
DMC_API short __stdcall dmc_conti_vmove_stop(WORD CardNo, WORD Crd, WORD axis, double dec, long imark);

//登入sn20191101
DMC_API short __stdcall dmc_enter_password_ex(WORD CardNo, const char *str_pass);

//电子齿轮功能
DMC_API short __stdcall dmc_gear_in(WORD CardNo, WORD master_axis, WORD slave_axis, WORD follow_source, double ratio_numerator,
                                    double ratio_denominator, double acc, double dec, double s_time, WORD buffer_mode);
DMC_API short __stdcall dmc_get_gear_in(WORD CardNo, WORD *master_axis, WORD slave_axis, WORD *follow_source,
                                        double *ratio_numerator, double *ratio_denominator, double *acc, double *dec,
                                        double *s_time, WORD *buffer_mode);
DMC_API short __stdcall dmc_update_gear_scale(WORD CardNo, WORD slave_axis, double ratio_numerator, double ratio_denominator,
                                              double acc, double dec, double s_time);
DMC_API short __stdcall dmc_gear_in_pos(WORD CardNo, WORD master_axis, WORD slave_axis, WORD follow_source,
                                        double ratio_numerator, double ratio_denominator, double master_sync_pos,
                                        double slave_sync_pos, double master_start_dist, double velocity, double acc, double dec,
                                        double s_time, WORD buffer_mode);
DMC_API short __stdcall dmc_get_gear_in_pos(WORD CardNo, WORD *master_axis, WORD slave_axis, WORD *follow_source,
                                            double *ratio_numerator, double *ratio_denominator, double *master_sync_pos,
                                            double *slave_sync_pos, double *master_start_dist, double *velocity, double *acc,
                                            double *dec, double *s_time, WORD *buffer_mode);
DMC_API short __stdcall dmc_get_in_gear_state(WORD CardNo, WORD slave_axis, WORD *in_gear);
DMC_API short __stdcall dmc_get_gear_aborted_state(WORD CardNo, WORD slave_axis, WORD *aborted_state);
DMC_API short __stdcall dmc_gear_out(WORD CardNo, WORD slave_axis);
DMC_API short __stdcall dmc_trace_set_config(WORD CardNo, short trace_cycle, short lost_handle, short trace_type,
                                             short trigger_object_index, short trigger_type, int mask, long long condition);
DMC_API short __stdcall dmc_trace_get_config(WORD CardNo, short *trace_cycle, short *lost_handle, short *trace_type,
                                             short *trigger_object_index, short *trigger_type, int *mask, long long *condition);

/***********************************************************
 * 配置采集对象，一次可以添加500个采集对象
 * data_type 	数据的类型，见采集对象说明。
 * data_index 	数据的主索引，如果是跟轴相关，则是轴序号，如果是IO，则是IO序号，如此类推
 * data_sub_index 数据的子索引，如果是按组采集IO，则表示IO结束的序号。
 * data_bytes 	对象字节数，现有采集类型会自动匹配，固定为0，预留后续扩展功能
 **********************************************************/
DMC_API short __stdcall dmc_trace_reset_config_object(WORD CardNo);
DMC_API short __stdcall dmc_trace_add_config_object(WORD CardNo, short data_type, int data_index, int data_sub_index,
                                                    short slave_id, short data_bytes);
DMC_API short __stdcall dmc_trace_get_config_object(WORD CardNo, short object_index, short *data_type, int *data_index,
                                                    int *data_sub_index, short *slave_id, short *data_bytes);

//启动trace
DMC_API short __stdcall dmc_trace_data_start(WORD CardNo);

//停止trace
DMC_API short __stdcall dmc_trace_data_stop(WORD CardNo);

//复位trace，停止采集的时候才能调用，会清除已采集到的数据以及溢出标志位
DMC_API short __stdcall dmc_trace_data_reset(WORD CardNo);


//trace是否已经启动
DMC_API short __stdcall dmc_trace_get_flag(WORD CardNo, short *start_flag, short *triggered_flag, short *lost_flag);

/***********************************************************
 *读取采集状态
 * valid_num 	已采集但未被读取的数据个数
 * free_num 	剩余可用于保存采集数据的个数
 * object_total_bytes   采集对象总字节数
 * object_total_num 	采集对象总个数
 **********************************************************/
DMC_API short __stdcall dmc_trace_get_state(WORD CardNo, int *valid_num, int *free_num, int *object_total_bytes,
                                            int *object_total_num);

/***********************************************************
 *读取采集数据
 * bufsize 	数据缓冲区字节数
 * data[1024] 	数据缓冲区，
 * byte_size   读取的数据的字节数
 **********************************************************/
DMC_API short __stdcall dmc_trace_get_data(WORD CardNo, int bufsize, unsigned char *data, int *byte_size);

//trace复位溢出信号，只是复位标志位
DMC_API short __stdcall dmc_trace_reset_lost_flag(WORD CardNo);
DMC_API short __stdcall dmc_message_buffer_enable(WORD CardNo, WORD index, DWORD buffer_size, BYTE console_enable);
DMC_API short __stdcall dmc_message_buffer_disable(WORD CardNo, WORD index);
DMC_API short __stdcall dmc_message_buffer_get_data(WORD CardNo, WORD index, long bufsize, BYTE *data, DWORD *pbufsize);

DMC_API short __stdcall dmc_t_pmove_extern_softstart(WORD CardNo, WORD axis, double MidPos, double TargetPos, double start_Vel,
                                                     double Max_Vel, double stop_Vel, DWORD delay_ms, double Max_Vel2,
                                                     double stop_vel2, double acc_time, double dec_time, WORD posi_mode);

DMC_API short __stdcall dmc_t_pmove_extern_softstart_unit(WORD CardNo, WORD axis, double MidPos, double TargetPos,
                                                          double start_Vel, double Max_Vel, double stop_Vel, DWORD delay_ms,
                                                          double Max_Vel2, double stop_vel2, double acc_time, double dec_time,
                                                          WORD posi_mode);


//PVT_continuous接口.
//下发PVT continuous各节点数据
DMC_API short __stdcall dmc_pvt_table_continuous(WORD CardNo, WORD axis, DWORD count, double *pos, double *vel, double *percent,
                                                 double *vel_max, double *acc, double *dec);
//根据传入的参数，获取各个位置节点的时间
DMC_API short __stdcall dmc_pvt_continuous_calculate(WORD CardNo, WORD axis, double *time);
//开启PVT continuous 运动
DMC_API short __stdcall dmc_pvt_continuous_start(WORD CardNo, WORD axis_num, WORD *axis_list, double *start_delay_time);

//总线复位输出保持开关设置
DMC_API short __stdcall nmc_set_slave_output_retain(WORD CardNo, WORD Enable);
DMC_API short __stdcall nmc_get_slave_output_retain(WORD CardNo, WORD *Enable);

DMC_API short __stdcall dmc_m_add_mutiposTrig_singledown_seg_data(WORD CardNo, WORD group, WORD axis, double safePos,
                                                                  double Target_pos, WORD process_mode, WORD trigAxisNum,
                                                                  WORD *trigAxisList, double *trigPos, WORD *trigPosType,
                                                                  WORD *trigMode, DWORD mark);
DMC_API short __stdcall dmc_m_add_mutiposTrig_mutidown_seg_data(WORD CardNo, WORD group, WORD axisnum, WORD *axis_list,
                                                                double *safePos, double *Target_pos, WORD process_mode,
                                                                WORD trigAxisNum, WORD *trigAxisList, double *trigPos,
                                                                WORD *trigPosType, WORD *trigMode, DWORD mark);

//反馈位置误差允许范围设置
DMC_API short __stdcall dmc_cmd_buf_set_allow_error(WORD CardNo, WORD group, double allow_error);
DMC_API short __stdcall dmc_cmd_buf_get_allow_error(WORD CardNo, WORD group, double *allow_error);
//半径终点圆弧插补
DMC_API short __stdcall dmc_arc_move_radius_multicoor(WORD CardNo, WORD Crd, WORD *AxisList, double *Target_Pos,
                                                      double Arc_Radius, WORD Arc_Dir, long Circle, WORD posi_mode);
//三点圆弧
DMC_API short __stdcall dmc_arc_move_3points_multicoor(WORD CardNo, WORD Crd, WORD *AxisList, double *Target_Pos, double *Mid_Pos,
                                                       long Circle, WORD posi_mode);

//反馈位置误差允许范围设置
DMC_API short __stdcall dmc_m_set_encoder_error_allow(WORD CardNo, WORD group, double allow_error);
DMC_API short __stdcall dmc_m_get_encoder_error_allow(WORD CardNo, WORD group, double *allow_error);

//掉电保持
DMC_API short __stdcall dmc_set_persistent_reg_byte(WORD CardNo, DWORD start, DWORD inum, const char *pdata);
DMC_API short __stdcall dmc_get_persistent_reg_byte(WORD CardNo, DWORD start, DWORD inum, char *pdata);
DMC_API short __stdcall dmc_set_persistent_reg_float(WORD CardNo, DWORD start, DWORD inum, const float *pdata);
DMC_API short __stdcall dmc_get_persistent_reg_float(WORD CardNo, DWORD start, DWORD inum, float *pdata);
DMC_API short __stdcall dmc_set_persistent_reg_int(WORD CardNo, DWORD start, DWORD inum, const int *pdata);
DMC_API short __stdcall dmc_get_persistent_reg_int(WORD CardNo, DWORD start, DWORD inum, int *pdata);

DMC_API short __stdcall nmc_torque_set_delay_cycle(WORD CardNo, WORD axis, int delay_cycle);

DMC_API short __stdcall dmc_conti_delay_outbit_to_start_path(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double delay_value,
                                                             WORD delay_mode, double ReverseTime);
DMC_API short __stdcall dmc_conti_delay_outbit_to_stop_path(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double delay_time,
                                                            double ReverseTime);
DMC_API short __stdcall dmc_conti_ahead_outbit_to_stop_path(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double ahead_value,
                                                            WORD ahead_mode, double ReverseTime);

//轴参数配置写flash接口
DMC_API short __stdcall dmc_set_persistent_param_config(WORD CardNo, WORD axis, DWORD item);
DMC_API short __stdcall dmc_get_persistent_param_config(WORD CardNo, WORD axis, DWORD *item);

DMC_API short __stdcall dmc_hcmp_fifo_add_point_dir_unit(WORD CardNo, WORD hcmp, WORD num, double *cmp_pos, DWORD dir);
DMC_API short __stdcall dmc_hcmp_fifo_add_table_dir(WORD CardNo, WORD hcmp, WORD num, double *cmp_pos, DWORD dir);

DMC_API short __stdcall dmc_axis_io_status_ex(WORD CardNo, WORD axis, DWORD *state); //读取指定轴有关运动信号的状态
DMC_API short __stdcall dmc_check_done_ex(WORD CardNo, WORD axis, WORD *state);      //读取指定轴的运动状态

DMC_API short __stdcall dmc_conti_line_section_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *pPosList,
                                                    double Section_Dist, WORD Bitno, WORD On_Off, WORD Io_Mode,
                                                    double Time_Dist_Value, double ReverseTime, WORD posi_mode, WORD mark);

DMC_API short __stdcall dmc_conti_arc_move_center_section_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList,
                                                               double *Target_Pos, double *Cen_Pos, WORD Arc_Dir, WORD Circle,
                                                               double Section_Dist, WORD Bitno, WORD On_Off, WORD Io_Mode,
                                                               double Time_Dist_Value, double ReverseTime, WORD posi_mode,
                                                               WORD mark);

DMC_API short __stdcall dmc_conti_arc_move_radius_section_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList,
                                                               double *Target_Pos, double Arc_Radius, WORD Arc_Dir, WORD Circle,
                                                               double Section_Dist, WORD Bitno, WORD On_Off, WORD Io_Mode,
                                                               double Time_Dist_Value, double ReverseTime, WORD posi_mode,
                                                               WORD mark);

DMC_API short __stdcall dmc_conti_arc_move_3points_section_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList,
                                                                double *Target_Pos, double *Mid_Pos, WORD Circle,
                                                                double Section_Dist, WORD Bitno, WORD On_Off, WORD Io_Mode,
                                                                double Time_Dist_Value, double ReverseTime, WORD posi_mode,
                                                                WORD mark);

DMC_API short __stdcall dmc_get_firmware_boot_type(WORD CardNo, WORD *boot_type);


//中断功能
DMC_API DWORD __stdcall dmc_int_enable(WORD CardNo, DMC3K5K_OPERATE funcIntHandler, void *operate_data);
DMC_API DWORD __stdcall dmc_int_disable(WORD CardNo);
DMC_API short __stdcall dmc_set_intmode_enable(WORD Cardno, WORD Intno, WORD Enable);
DMC_API short __stdcall dmc_get_intmode_enable(WORD Cardno, WORD Intno, WORD *Status);
DMC_API short __stdcall dmc_set_intmode_config(WORD Cardno, WORD Intno, WORD IntItem, WORD IntIndex, WORD IntSubIndex,
                                               WORD Logic);
DMC_API short __stdcall dmc_get_intmode_config(WORD Cardno, WORD Intno, WORD *IntItem, WORD *IntIndex, WORD *IntSubIndex,
                                               WORD *Logic);
DMC_API short __stdcall dmc_get_int_status(WORD Cardno, DWORD *IntStatus);
DMC_API short __stdcall dmc_reset_int_status(WORD Cardno, WORD Intno);

DMC_API short __stdcall dmc_set_pci_int(WORD CardNo);
DMC_API short __stdcall dmc_pmove_change_pos_speed_inbit(WORD CardNo, WORD axis, WORD inbit, WORD enable);
DMC_API short __stdcall dmc_get_pmove_change_pos_speed_inbit(WORD CardNo, WORD axis, WORD *inbit, WORD *enable);

DMC_API short __stdcall dmc_set_arc_zone_limit_config_unit(WORD CardNo, WORD *AxisList, WORD AxisNum, double *Center,
                                                           double Radius, WORD Source, WORD StopMode);
DMC_API short __stdcall dmc_get_arc_zone_limit_config_unit(WORD CardNo, WORD *AxisList, WORD *AxisNum, double *Center,
                                                           double *Radius, WORD *Source, WORD *StopMode);

DMC_API short __stdcall dmc_set_latch_stop_axis(WORD CardNo, WORD latch, WORD num, WORD *axislist);
DMC_API short __stdcall dmc_get_latch_stop_axis(WORD CardNo, WORD latch, WORD *num, WORD *axislist);
DMC_API short __stdcall dmc_compare_add_point_cycle_2d(WORD ConnectNo, WORD *axis, double *pos, WORD *dir, DWORD bitno,
                                                       DWORD cycle, WORD level);

DMC_API short __stdcall dmc_set_factor_error_unit(WORD CardNo, WORD axis, double factor, double error);
DMC_API short __stdcall dmc_get_factor_error_unit(WORD CardNo, WORD axis, double *factor, double *error);
DMC_API short __stdcall dmc_set_pulse_encoder_count_error_unit(WORD CardNo, WORD axis, double error);
DMC_API short __stdcall dmc_get_pulse_encoder_count_error_unit(WORD CardNo, WORD axis, double *error);
DMC_API short __stdcall dmc_check_pulse_encoder_count_error_unit(WORD CardNo, WORD axis, double *pulse_position,
                                                                 double *enc_position);

DMC_API short __stdcall dmc_set_ad_monitor_config(WORD CardNo, WORD Crd, WORD CANid, WORD channel, WORD ADEn, double ADValDown,
                                                  double ADValUp);
DMC_API short __stdcall dmc_get_ad_monitor_config(WORD CardNo, WORD Crd, WORD *CANid, WORD *channel, WORD *ADEn,
                                                  double *ADValDown, double *ADValUp);
DMC_API short __stdcall dmc_get_ad_monitor_result(WORD CardNo, WORD Crd, WORD *ch, double *ADRet, WORD *num, double *pos);
DMC_API short __stdcall dmc_clear_ad_monitor_result(WORD CardNo, WORD Crd);


DMC_API short __stdcall dmc_update_target_position_extern_unit(WORD CardNo, WORD axis, double mid_pos, double aim_pos, double vel,
                                                               WORD posi_mode);
//指定轴做定长位移运动 同时发送速度和S时间(当量)
DMC_API short __stdcall dmc_pmove_extern_unit(WORD CardNo, WORD axis, double dist, double Min_Vel, double Max_Vel, double Tacc,
                                              double Tdec, double stop_Vel, double s_para, WORD posi_mode);
DMC_API short __stdcall dmc_pmove_extern_acc_unit(WORD CardNo, WORD axis, double dist, double Min_Vel, double Max_Vel,
                                                  double Tacc, double Tdec, double stop_Vel, double s_para, WORD posi_mode);
DMC_API short __stdcall dmc_m_mutiposTrig_outbit(WORD CardNo, WORD group, WORD bitno, WORD on_off, WORD process_mode,
                                                 WORD trigAxisNum, WORD *trigAxisList, double *trigPos, WORD *trigPosType,
                                                 WORD *trigMode, DWORD mark); //位置触发IO输出

DMC_API short __stdcall dmc_cmp_fifo_set_enable(WORD CardNo, WORD Crd, WORD enable);
DMC_API short __stdcall dmc_cmp_fifo_get_enable(WORD CardNo, WORD Crd, WORD *enable);
DMC_API short __stdcall dmc_cmp_fifo_get_state(WORD CardNo, WORD Crd, long *remained_space);
DMC_API short __stdcall dmc_cmp_fifo_clear_points(WORD CardNo, WORD Crd);
DMC_API short __stdcall dmc_cmp_fifo_set_config_params(WORD CardNo, WORD Crd, WORD Bitno, WORD On_Off, WORD Io_Mode,
                                                       double Time_Dist_Value, double ReverseTime);
DMC_API short __stdcall dmc_cmp_fifo_get_config_params(WORD CardNo, WORD Crd, WORD *Bitno, WORD *On_Off, WORD *Io_Mode,
                                                       double *Time_Dist_Value, double *ReverseTime);
DMC_API short __stdcall dmc_conti_line_add_cmp_fifo_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                         double *cmp_pos, WORD num, WORD posi_mode, long mark);
DMC_API short __stdcall dmc_conti_arc_move_3points_add_cmp_fifo_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList,
                                                                     double *Target_Pos, double *Mid_Pos, WORD Circle,
                                                                     double *cmp_pos, WORD num, WORD posi_mode, long mark);
DMC_API short __stdcall dmc_conti_arc_move_center_add_cmp_fifo_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList,
                                                                    double *Target_Pos, double *Cen_Pos, WORD Arc_Dir,
                                                                    WORD Circle, double *cmp_pos, WORD num, WORD posi_mode,
                                                                    long mark);
DMC_API short __stdcall dmc_conti_arc_move_radius_add_cmp_fifo_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList,
                                                                    double *Target_Pos, double Arc_Radius, WORD Arc_Dir,
                                                                    WORD Circle, double *cmp_pos, WORD num, WORD posi_mode,
                                                                    long mark);
DMC_API short __stdcall dmc_cmp_fifo_get_total_point(WORD CardNo, WORD Crd, long *total_point);
DMC_API short __stdcall dmc_cmp_fifo_get_remain_point(WORD CardNo, WORD Crd, long *remain_point);
DMC_API short __stdcall dmc_cmp_fifo_get_trig_point(WORD CardNo, WORD Crd, long *trig_point);
DMC_API short __stdcall dmc_cmp_fifo_get_force_trig_point(WORD CardNo, WORD Crd, long *force_trig_point);

DMC_API short __stdcall dmc_conti_wait_node_input_delay_to_start(WORD CardNo, WORD Crd, WORD node_ID, WORD bitno, WORD on_off,
                                                                 double delay_value, WORD delay_mode, double TimeOut);
DMC_API short __stdcall dmc_conti_wait_node_input_ahead_to_stop(WORD CardNo, WORD Crd, WORD node_ID, WORD bitno, WORD on_off,
                                                                double ahead_value, WORD ahead_mode, double TimeOut);
DMC_API short __stdcall dmc_conti_delay_node_outbit_to_start(WORD CardNo, WORD Crd, WORD node_ID, WORD bitno, WORD on_off,
                                                             double delay_value, WORD delay_mode, double ReverseTime);
DMC_API short __stdcall dmc_conti_delay_node_outbit_to_stop(WORD CardNo, WORD Crd, WORD node_ID, WORD bitno, WORD on_off,
                                                            double delay_time, double ReverseTime);
DMC_API short __stdcall dmc_conti_ahead_node_outbit_to_stop(WORD CardNo, WORD Crd, WORD node_ID, WORD bitno, WORD on_off,
                                                            double ahead_value, WORD ahead_mode, double ReverseTime);
DMC_API short __stdcall dmc_conti_write_node_outbit(WORD CardNo, WORD Crd, WORD node_ID, WORD bitno, WORD on_off,
                                                    double ReverseTime);
DMC_API short __stdcall dmc_conti_clear_node_io_action(WORD CardNo, WORD Crd, WORD node_ID, DWORD Io_Mask);


DMC_API short __stdcall dmc_get_connect_type(WORD CardNo, WORD *ConnectType);
DMC_API short __stdcall dmc_board_init_eth(WORD CardNo, const char *IpAddr);
//减速停止距离
DMC_API short __stdcall dmc_set_dec_stop_dist_unit(WORD CardNo, WORD axis, double dist);
DMC_API short __stdcall dmc_get_dec_stop_dist_unit(WORD CardNo, WORD axis, double *dist);
//最大速度限制设置(脉冲当量)
DMC_API short __stdcall dmc_set_profile_limit_unit(WORD CardNo, WORD axis, double Limit_Max_Vel, double Limit_Max_Acc,
                                                   double EvenTime);
DMC_API short __stdcall dmc_get_profile_limit_unit(WORD CardNo, WORD axis, double *Limit_Max_Vel, double *Limit_MAX_Acc,
                                                   double *EvenTime);
DMC_API short __stdcall dmc_set_vector_profile_limit_unit(WORD CardNo, WORD Crd, double Limit_Max_Vel, double Limit_Max_Acc,
                                                          double EvenTime);
DMC_API short __stdcall dmc_get_vector_profile_limit_unit(WORD CardNo, WORD Crd, double *Limit_Max_Vel, double *Limit_MAX_Acc,
                                                          double *EvenTime);
//启用单轴限速功能
DMC_API short __stdcall dmc_set_vector_profile_limit_by_axis(WORD CardNo, WORD Crd, WORD Enable);
DMC_API short __stdcall dmc_get_vector_profile_limit_by_axis(WORD CardNo, WORD Crd, WORD *Enable);
DMC_API short __stdcall dmc_get_axis_follow_line_enable(WORD CardNo, WORD Crd, WORD *enable_flag);

DMC_API short __stdcall dmc_set_counter_reverse(WORD CardNo, WORD axis, WORD reverse);
DMC_API short __stdcall dmc_get_counter_reverse(WORD CardNo, WORD axis, WORD *reverse);
DMC_API short __stdcall dmc_set_extra_counter_reverse(WORD CardNo, WORD axis, WORD reverse);
DMC_API short __stdcall dmc_get_extra_counter_reverse(WORD CardNo, WORD axis, WORD *reverse);

DMC_API short __stdcall dmc_conti_stop_axis(WORD CardNo, WORD Crd, WORD axis, double dec, int imark);

//读取插补长度
DMC_API short __stdcall dmc_read_vector_length_unit(WORD CardNo, WORD Crd, double *total_length, double *left_length);
/*********************************************************************************************************
简易电子凸轮运动
*********************************************************************************************************/
DMC_API short __stdcall dmc_cam_table_unit(WORD CardNo, WORD MasterAxisNo, WORD SlaveAxisNo, DWORD Count, double *pMasterPos,
                                           double *pSlavePos, WORD SrcMode);
DMC_API short __stdcall dmc_cam_move(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_cam_move_cycle(WORD CardNo, WORD axis);

DMC_API short __stdcall dmc_conti_set_fairing_enable(WORD CardNo, WORD Crd, WORD enable, double fairing_length);
DMC_API short __stdcall dmc_conti_get_fairing_enable(WORD CardNo, WORD Crd, WORD *enable, double *fairing_length);

DMC_API short __stdcall dmc_set_eth_timeout(int timems);

DMC_API short __stdcall dmc_set_extra_encoder_extern(WORD CardNo, WORD channel, int pos);
DMC_API short __stdcall dmc_get_extra_encoder_extern(WORD CardNo, WORD channel, int *pos);

DMC_API short __stdcall dmc_conti_smooth_contour_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, WORD point_num,
                                                      double *x, double *y, double *z, double vel_coef, double eps, long mark);
DMC_API short __stdcall dmc_get_conti_smooth_contour_curve(WORD point_num, double *x, double *y, double *z, double eps,
                                                           double *curve_x, double *curve_y, double *curve_z, double *length);


//新增针对模块高速比较指令
DMC_API short __stdcall nmc_hcmp_set_mode(WORD CardNo, WORD PortNum, WORD nodenum, WORD hcmp, WORD cmp_mode);
DMC_API short __stdcall nmc_hcmp_get_mode(WORD CardNo, WORD PortNum, WORD nodenum, WORD hcmp, WORD *cmp_mode);
DMC_API short __stdcall nmc_hcmp_set_config(WORD CardNo, WORD PortNum, WORD nodenum, WORD hcmp, WORD channel, WORD cmp_source,
                                            WORD cmp_logic, long time);
DMC_API short __stdcall nmc_hcmp_get_config(WORD CardNo, WORD PortNum, WORD nodenum, WORD hcmp, WORD *channel, WORD *cmp_source,
                                            WORD *cmp_logic, long *time);
DMC_API short __stdcall nmc_hcmp_clear_points(WORD CardNo, WORD PortNum, WORD nodenum, WORD hcmp, WORD enable);
DMC_API short __stdcall nmc_hcmp_add_point(WORD CardNo, WORD PortNum, WORD nodenum, WORD hcmp, long cmp_pos);
DMC_API short __stdcall nmc_hcmp_set_liner(WORD CardNo, WORD PortNum, WORD nodenum, WORD hcmp, long Increment, long Count);
DMC_API short __stdcall nmc_hcmp_get_liner(WORD CardNo, WORD PortNum, WORD nodenum, WORD hcmp, long *Increment, long *Count);
DMC_API short __stdcall nmc_hcmp_get_current_state(WORD CardNo, WORD PortNum, WORD nodenum, WORD hcmp, long *remained_points,
                                                   long *current_point, long *runned_points);

//样条曲线相关
DMC_API short __stdcall dmc_set_axis_follow_trajectory_displacement(WORD CardNo, WORD crd, WORD num, WORD *Axis_list);
DMC_API short __stdcall dmc_get_axis_follow_trajectory_displacement(WORD CardNo, WORD crd, WORD *num, WORD *Axis_list);
DMC_API short __stdcall dmc_set_tool_length_compensation_param(WORD CardNo, WORD axis, double length);
DMC_API short __stdcall dmc_get_tool_length_compensation_param(WORD CardNo, WORD axis, double *length);
DMC_API short __stdcall dmc_set_tool_length_compensation_enable(WORD CardNo, WORD axis, WORD enable);
DMC_API short __stdcall dmc_get_tool_length_compensation_enable(WORD CardNo, WORD axis, WORD *enable);
DMC_API short __stdcall dmc_set_normal_direction_control(WORD CardNo, WORD crd, WORD axis, WORD mode);
DMC_API short __stdcall dmc_get_normal_direction_control(WORD CardNo, WORD crd, WORD *axis, WORD *mode);
DMC_API short __stdcall dmc_set_gap_cmp_param(WORD CardNo, WORD crd, WORD pin, WORD logic, WORD mode, WORD auxi_axis, WORD source,
                                              long rev_time, double *gap);
DMC_API short __stdcall dmc_get_gap_cmp_param(WORD CardNo, WORD crd, WORD *pin, WORD *logic, WORD *mode, WORD *auxi_axis,
                                              WORD *source, long *rev_time, double *gap);
DMC_API short __stdcall dmc_set_gap_cmp_enable(WORD CardNo, WORD crd, WORD enable);
DMC_API short __stdcall dmc_get_gap_cmp_enable(WORD CardNo, WORD crd, WORD *enable);
DMC_API short __stdcall dmc_set_normal_direction_control_enable(WORD CardNo, WORD crd, WORD enable);
DMC_API short __stdcall dmc_get_normal_direction_control_enable(WORD CardNo, WORD crd, WORD *enable);

DMC_API short __stdcall dmc_mc_gear_in(WORD CardNo, WORD slave_axis, WORD master_axis, WORD execute, WORD conti_update,
                                       WORD master_source, double ratio_numerator, double ratio_denominator, double acc,
                                       double dec, double jerk, WORD buffer_mode);
DMC_API short __stdcall dmc_get_mc_gear_in(WORD CardNo, WORD slave_axis, WORD *master_axis, WORD *execute, WORD *conti_update,
                                           WORD *master_source, double *ratio_numerator, double *ratio_denominator, double *acc,
                                           double *dec, double *jerk, WORD *buffer_mode);
DMC_API short __stdcall dmc_get_mc_gearin_status(WORD CardNo, WORD slave_axis, WORD *in_gear, WORD *busy, WORD *active,
                                                 WORD *cmd_aborted, WORD *error, DWORD *error_id);
DMC_API short __stdcall dmc_mc_gear_out(WORD CardNo, WORD slave_axis, WORD *execute);
DMC_API short __stdcall dmc_get_mc_gear_out_status(WORD CardNo, WORD slave_axis, WORD *done, WORD *busy, WORD *cmd_aborted,
                                                   WORD *error, DWORD *error_id);
DMC_API short __stdcall dmc_mc_combine_axes(WORD CardNo, WORD slave_axis, WORD master_axis1, WORD master_axis2, WORD execute,
                                            WORD conti_update, WORD master_source1, WORD master_source2, WORD combine_mode,
                                            double ratio_numerator1, double ratio_denominator1, double ratio_numerator2,
                                            double ratio_denominator2, double acc, double dec, double jerk, WORD buffer_mode);
DMC_API short __stdcall dmc_get_mc_combine_axes(WORD CardNo, WORD slave_axis, WORD *master_axis1, WORD *master_axis2,
                                                WORD *execute, WORD *conti_update, WORD *master_source1, WORD *master_source2,
                                                WORD *combine_mode, double *ratio_numerator1, double *ratio_denominator1,
                                                double *ratio_numerator2, double *ratio_denominator2, double *acc, double *dec,
                                                double *jerk, WORD *buffer_mode);
DMC_API short __stdcall dmc_get_mc_combine_axes_status(WORD CardNo, WORD slave_axis, WORD *in_sync, WORD *busy, WORD *active,
                                                       WORD *cmd_aborted, WORD *error, DWORD *error_id);

DMC_API short __stdcall dmc_set_space_collision_zone_param(WORD CardNo, WORD axis_num, WORD *axis_list, WORD zone_num,
                                                           double *neg_limit, double *pos_limit, WORD stop_mode, WORD pos_source);
DMC_API short __stdcall dmc_get_space_collision_zone_param(WORD CardNo, WORD *axis_num, WORD *axis_list, WORD *zone_num,
                                                           double *neg_limit, double *pos_limit, WORD *stop_mode,
                                                           WORD *pos_source);
DMC_API short __stdcall dmc_set_space_collision_zone_enable(WORD CardNo, WORD enable);
DMC_API short __stdcall dmc_get_space_collision_zone_enable(WORD CardNo, WORD *enable);

//批量读取 不带当量20201016
DMC_API short __stdcall dmc_get_position_extern(WORD CardNo, WORD axis, double *pos);
DMC_API short __stdcall dmc_get_encoder_extern(WORD CardNo, WORD axis, double *pos);
DMC_API short __stdcall dmc_read_current_speed_extern(WORD CardNo, WORD axis, double *current_speed);

//电子凸轮
DMC_API short __stdcall dmc_cam_in(WORD CardNo, WORD slave_axis, WORD master_axis, WORD execute, WORD conti_update,
                                   WORD cam_table, WORD periodic, WORD master_abs, WORD slave_abs, double master_offset,
                                   double slave_offset, double master_scaling, double slave_scaling, double master_start_dist,
                                   double master_sync_pos, double active_pos, WORD active_mode, WORD start_mode, double velocity,
                                   double acc, double dec, double jerk, WORD master_source, WORD buffer_mode);
DMC_API short __stdcall dmc_get_cam_in_status(WORD CardNo, WORD slave_axis, WORD *in_sync, WORD *end_of_profile, WORD *busy,
                                              WORD *active, WORD *cmd_aborted, WORD *error, DWORD *error_id);
DMC_API short __stdcall dmc_cam_out(WORD CardNo, WORD slave_axis, WORD execute);
DMC_API short __stdcall dmc_get_cam_out_status(WORD CardNo, WORD slave_axis, WORD *done, WORD *busy, WORD *cmd_aborted,
                                               WORD *error, DWORD *error_id);
DMC_API short __stdcall dmc_cam_read_point(WORD CardNo, WORD execute, WORD cam_table, WORD cam_chg_point, DWORD cam_point_num,
                                           WORD *done, WORD *busy, WORD *error, DWORD *error_id, double *master_pos,
                                           double *slave_pos, double *slave_vel, double *slave_acc, double *slave_jerk,
                                           WORD *type);
DMC_API short __stdcall dmc_cam_write_point(WORD CardNo, WORD execute, WORD cam_table, DWORD cam_point_num, double master_pos,
                                            double slave_pos, double slave_vel, double slave_acc, double slave_jerk, WORD type,
                                            WORD *done, WORD *busy, WORD *error, DWORD *error_id);
DMC_API short __stdcall dmc_cam_set(WORD CardNo, WORD execute, WORD cam_table, WORD *done, WORD *busy, WORD *error,
                                    DWORD *error_id);
DMC_API short __stdcall dmc_cam_read_tappet_status(WORD CardNo, WORD execute, WORD cam_table, DWORD tappet_num1,
                                                   DWORD tappet_num2, DWORD tappet_num3, DWORD tappet_num4, DWORD tappet_num5,
                                                   DWORD tappet_num6, DWORD tappet_num7, DWORD tappet_num8, WORD *valid,
                                                   WORD *busy, WORD *error, DWORD *error_id, WORD *status1, WORD *status2,
                                                   WORD *status3, WORD *status4, WORD *status5, WORD *status6, WORD *status7,
                                                   WORD *status8);
DMC_API short __stdcall dmc_cam_read_tappet_value(WORD CardNo, WORD execute, WORD cam_table, DWORD tappet_num, WORD *valid,
                                                  WORD *busy, WORD *error, DWORD *error_id, double *master_pos,
                                                  WORD *positive_mode, WORD *negative_mode);
DMC_API short __stdcall dmc_cam_write_tappet_value(WORD CardNo, WORD execute, WORD cam_table, DWORD tappet_num, double master_pos,
                                                   WORD positive_mode, WORD negative_mode, WORD *done, WORD *busy, WORD *error,
                                                   DWORD *error_id);
DMC_API short __stdcall dmc_cam_add_tappet(WORD CardNo, WORD execute, WORD cam_table, double master_pos, WORD positive_mode,
                                           WORD negative_mode, WORD *done, WORD *busy, WORD *error, DWORD *error_id,
                                           DWORD *tappet_num);
DMC_API short __stdcall dmc_cam_delete_tappet(WORD CardNo, WORD execute, WORD cam_table, WORD *done, WORD *busy, WORD *error,
                                              DWORD *error_id);

DMC_API short __stdcall dmc_cmd_buf_temp_delete(WORD CardNo, WORD group, WORD addr, WORD num, WORD delete_mode);

DMC_API short __stdcall dmc_conti_set_wait_mode(WORD CardNo, WORD Crd, WORD wait_mode);
DMC_API short __stdcall dmc_conti_get_wait_mode(WORD CardNo, WORD Crd, WORD *wait_mode);

DMC_API short __stdcall dmc_set_peak_config(WORD CardNo, WORD axis, WORD enable, double u_time);
DMC_API short __stdcall dmc_get_peak_config(WORD CardNo, WORD axis, WORD *enable, double *u_time);

DMC_API short __stdcall dmc_set_axis_err_band(WORD CardNo, WORD axis, double err_band, WORD set_cycle);
DMC_API short __stdcall dmc_get_axis_err_band(WORD CardNo, WORD axis, double *err_band, WORD *set_cycle);
DMC_API short __stdcall dmc_set_axis_err_band_unit(WORD CardNo, WORD axis, double err_band, WORD set_cycle);
DMC_API short __stdcall dmc_get_axis_err_band_unit(WORD CardNo, WORD axis, double *err_band, WORD *set_cycle);

//回读pmove运动规划总时间及剩余时间
DMC_API short __stdcall dmc_get_axis_plan_time_info(WORD CardNo, WORD axis, double *sum_time, double *remain_time);
//设置插补单轴最大允许速度值
DMC_API short __stdcall dmc_set_axis_max_interpo_speed(WORD CardNo, WORD axis, double max_speed);
//回读插补单轴最大允许速度值
DMC_API short __stdcall dmc_get_axis_max_interpo_speed(WORD CardNo, WORD axis, double *max_speed);

DMC_API short __stdcall dmc_set_axis_max_interpo_speed_enable(WORD CardNo, WORD axis, WORD enable);
DMC_API short __stdcall dmc_get_axis_max_interpo_speed_enable(WORD CardNo, WORD axis, WORD *enable);

DMC_API short __stdcall dmc_set_diagnosis_log_enable(WORD CardNo, WORD Crd, WORD enable);
DMC_API short __stdcall dmc_get_diagnosis_log_enable(WORD CardNo, WORD Crd, WORD *enable);
DMC_API short __stdcall dmc_get_diagnosis_log_data(WORD CardNo, WORD Crd);

DMC_API short __stdcall nmc_reverse_outbit(WORD CardNo, WORD Channel, WORD NoteID, WORD IoBit, double reverse_time);

DMC_API short __stdcall dmc_sine_oscillate(WORD CardNo, WORD Axis, double Amplitude, double Frequency);
DMC_API short __stdcall dmc_sine_oscillate_stop(WORD CardNo, WORD Axis);

DMC_API short __stdcall dmc_set_apf_rotary_cut_init(WORD CardNo, DWORD rotary_cut_id, WORD execute, double rotary_axis_radius,
                                                    DWORD rotary_axis_knife_num, double feed_axis_radius, double cutlength,
                                                    double sync_start_pos, double sync_stop_pos, double rot_start_pos,
                                                    double fed_stop_pos);
DMC_API short __stdcall dmc_get_apf_rotary_cut_init(WORD CardNo, DWORD rotary_cut_id, WORD *execute, double *rotary_axis_radius,
                                                    DWORD *rotary_axis_knife_num, double *feed_axis_radius, double *cutlength,
                                                    double *sync_start_pos, double *sync_stop_pos, double *rot_start_pos,
                                                    double *fed_stop_pos);
DMC_API short __stdcall dmc_get_apf_rotary_cut_init_status(WORD CardNo, DWORD rotary_cut_id, WORD *done, WORD *busy, WORD *error,
                                                           DWORD *error_id);
DMC_API short __stdcall dmc_apf_rotary_cut_in(WORD CardNo, DWORD rotary_cut_id, WORD execute, WORD rotary_axis, WORD feed_axis);
DMC_API short __stdcall dmc_get_apf_rotary_cut_in_status(WORD CardNo, DWORD rotary_cut_id, WORD *done, WORD *busy, WORD *error,
                                                         DWORD *error_id);
DMC_API short __stdcall dmc_get_apf_rotary_cut_in(WORD CardNo, DWORD rotary_cut_id, WORD *execute, WORD *rotary_axis,
                                                  WORD *feed_axis);
DMC_API short __stdcall dmc_apf_rotary_cut_out(WORD CardNo, DWORD rotary_cut_id, WORD execute, WORD rotary_axis);
DMC_API short __stdcall dmc_get_apf_rotary_cut_out_status(WORD CardNo, DWORD rotary_cut_id, WORD *done, WORD *busy, WORD *error,
                                                          DWORD *error_id);

DMC_API short __stdcall dmc_conti_set_clear_current_mark_mode(WORD CardNo, WORD Crd, WORD mode);
DMC_API short __stdcall dmc_conti_get_clear_current_mark_mode(WORD CardNo, WORD Crd, WORD *mode);
DMC_API short __stdcall dmc_conti_clear_current_mark(WORD CardNo, WORD Crd);

DMC_API short __stdcall dmc_conti_set_arc_translate_mode(WORD CardNo, WORD Crd, WORD mode);
DMC_API short __stdcall dmc_conti_get_arc_translate_mode(WORD CardNo, WORD Crd, WORD *mode);
DMC_API short __stdcall dmc_trace_set_source(WORD CardNo, WORD source);

DMC_API short __stdcall nmc_sync_set_profile_unit(WORD CardNo, WORD AxisNum, WORD *AxisList, double *Min_Vel, double *Max_Vel,
                                                  double *Tacc, double *Tdec, double *Stop_Vel);

DMC_API short __stdcall nmc_write_rxpdo_extra_short(WORD CardNo, WORD PortNum, WORD address, WORD DataLen, WORD *Value);
DMC_API short __stdcall nmc_read_rxpdo_extra_short(WORD CardNo, WORD PortNum, WORD address, WORD DataLen, WORD *Value);
DMC_API short __stdcall nmc_read_txpdo_extra_short(WORD CardNo, WORD PortNum, WORD address, WORD DataLen, WORD *Value);

DMC_API short __stdcall dmc_set_timeout(WORD CardNo, DWORD timems);
DMC_API short __stdcall nmc_sync_pos_change_mode(WORD CardNo, WORD portno, WORD axis);

DMC_API short __stdcall dmc_mc_gear_in_pos(WORD CardNo, WORD slave_axis, WORD master_axis, WORD execute, WORD conti_update,
                                           WORD master_source, double ratio_numerator, double ratio_denominator,
                                           double master_sync_pos, double slave_sync_pos, double master_start_dist,
                                           WORD buffer_mode);
DMC_API short __stdcall dmc_get_mc_gear_in_pos(WORD CardNo, WORD slave_axis, WORD *master_axis, WORD *execute, WORD *conti_update,
                                               WORD *master_source, double *ratio_numerator, double *ratio_denominator,
                                               double *master_sync_pos, double *slave_sync_pos, double *master_start_dist,
                                               WORD *buffer_mode);
DMC_API short __stdcall dmc_get_mc_gear_in_pos_status(WORD CardNo, WORD slave_axis, WORD *start_sync, WORD *in_sync, WORD *busy,
                                                      WORD *active, WORD *cmd_aborted, WORD *error, DWORD *error_id);

DMC_API short __stdcall dmc_get_watchdog_trig_status(WORD CardNo, WORD *status);
DMC_API short __stdcall dmc_reset_watchdog_trig_status(WORD CardNo);
DMC_API short __stdcall dmc_conti_set_transarc_io_insert_mode(WORD CardNo, WORD Crd, WORD mode);
DMC_API short __stdcall dmc_conti_get_transarc_io_insert_mode(WORD CardNo, WORD Crd, WORD *mode);

DMC_API short __stdcall dmc_multi_axes_motion_sync_pmove_unit(WORD CardNo, WORD axis_num, WORD *axis_list, double *dist_list,
                                                              double *Min_Vel_list, double *Max_Vel_list, double *Tacc_list,
                                                              double *Tdec_list, double *stop_Vel_list, double *s_para_list,
                                                              WORD *posi_mode_list, WORD mode);

DMC_API short __stdcall dmc_set_ez_map_input(WORD CardNo, WORD axis, WORD enable, WORD mode, WORD index, WORD sub_index);
DMC_API short __stdcall dmc_get_ez_map_input(WORD CardNo, WORD axis, WORD *enable, WORD *mode, WORD *index, WORD *sub_index);
DMC_API short __stdcall nmc_set_etc_el_stop_mode(WORD CardNo, WORD axis, WORD el_control_mode, double diff_pos, DWORD filter);

DMC_API short __stdcall dmc_circle_move_center_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                    double *Cen_Pos, WORD Arc_Dir, long Circle, WORD posi_mode);
DMC_API short __stdcall dmc_conti_circle_move_center_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                          double *Cen_Pos, WORD Arc_Dir, long Circle, WORD posi_mode, long mark);
DMC_API short __stdcall dmc_set_acuate_angle_config_params(WORD CardNo, WORD Crd, double acuate_angle, double angle_trans_speed,
                                                           WORD enable);
DMC_API short __stdcall dmc_get_acuate_angle_config_params(WORD CardNo, WORD Crd, double *acuate_angle, double *angle_trans_speed,
                                                           WORD *enable);
DMC_API short __stdcall dmc_set_axes_link_params(WORD CardNo, WORD master, WORD slave);
DMC_API short __stdcall dmc_get_axes_link_params(WORD CardNo, WORD master, WORD *slave);
DMC_API short __stdcall dmc_remove_axes_link_params(WORD CardNo, WORD master);

DMC_API short __stdcall dmc_set_alm_mode_ex(WORD CardNo, WORD axis, WORD enable, WORD alm_logic, WORD alm_action,
                                            WORD alm_all); //新增配置
DMC_API short __stdcall dmc_get_alm_mode_ex(WORD CardNo, WORD axis, WORD *enable, WORD *alm_logic, WORD *alm_action,
                                            WORD *alm_all); //新增配置
//读取编码器方向
DMC_API short __stdcall dmc_get_encoder_dir(WORD CardNo, WORD axis, WORD *dir);

DMC_API short __stdcall dmc_download_configfile_ex(WORD CardNo, const char *FileName);

DMC_API short __stdcall dmc_set_s_profile_config(WORD CardNo, WORD axis, double acc_s_time, double dec_s_time);
DMC_API short __stdcall dmc_get_s_profile_config(WORD CardNo, WORD axis, double *acc_s_time, double *dec_s_time);

DMC_API short __stdcall nmc_set_slave_state(WORD CardNo, WORD SlaveId, WORD SlaveState);
DMC_API short __stdcall nmc_get_slave_state(WORD CardNo, WORD SlaveId, WORD *SlaveState);

DMC_API short __stdcall dmc_sync_pmove_unit(WORD CardNo, WORD axis_num, WORD *axis_list, double *dist_list, WORD *posi_mode_list,
                                            WORD mode);
DMC_API short __stdcall dmc_set_axis_abnormal_mode(WORD CardNo, WORD enable);
DMC_API short __stdcall dmc_get_axis_abnormal_mode(WORD CardNo, WORD *enable);
DMC_API short __stdcall dmc_clear_axis_abnormal_state(WORD CardNo, WORD axis, WORD count);
DMC_API short __stdcall dmc_set_coordinate_abnormal_mode(WORD CardNo, WORD enable);
DMC_API short __stdcall dmc_get_coordinate_abnormal_mode(WORD CardNo, WORD *enable);
DMC_API short __stdcall dmc_clear_crd_abnormal_state(WORD CardNo, WORD Crd, WORD count);
DMC_API short __stdcall dmc_set_coordinate_remainspace_mode(WORD CardNo, WORD Crd, WORD enable);
DMC_API short __stdcall dmc_get_coordinate_remainspace_mode(WORD CardNo, WORD Crd, WORD *enable);
DMC_API short __stdcall dmc_hcmp_add_linear_unit(WORD CardNo, WORD hcmp, int count, struct_hs_cmp_info *cmp_str);
DMC_API short __stdcall dmc_set_axis_handwheel_encoder_filter_frequancy(WORD CardNo, WORD axis, double frequancy);
DMC_API short __stdcall dmc_get_axis_handwheel_encoder_filter_frequancy(WORD CardNo, WORD axis, double *frequancy);

DMC_API short __stdcall nmc_set_slave_alias(WORD CardNo, WORD portnum, WORD auto_address, WORD alias_address);
DMC_API short __stdcall nmc_get_slave_alias(WORD CardNo, WORD portnum, WORD auto_address, WORD *alias_address);

DMC_API short __stdcall dmc_set_pwm_first_pulse_mode(WORD CardNo, WORD pwm_no, WORD enable);
DMC_API short __stdcall dmc_get_pwm_first_pulse_mode(WORD CardNo, WORD pwm_no, WORD *enable);
DMC_API short __stdcall dmc_set_pwm_first_pulse_duty(WORD CardNo, WORD pwm_no, double duty);
DMC_API short __stdcall dmc_get_pwm_first_pulse_duty(WORD CardNo, WORD pwm_no, double *duty);

DMC_API short __stdcall dmc_cmp_fifo_set_hcmp2d_pos_ratio(WORD CardNo, WORD Crd, WORD hcmp2d, double xpos_ratio,
                                                          double ypos_ratio);
DMC_API short __stdcall dmc_cmp_fifo_get_hcmp2d_pos_ratio(WORD CardNo, WORD Crd, WORD hcmp2d, double *xpos_ratio,
                                                          double *ypos_ratio);

DMC_API short __stdcall dmc_set_leadscrew_comp_datasheet_enable(WORD CardNo, WORD axis, WORD enable, int point_num);
DMC_API short __stdcall dmc_get_leadscrew_comp_datasheet_enable(WORD CardNo, WORD axis, WORD *enable, int *point_num);
DMC_API short __stdcall dmc_set_pos_calibrate_config(WORD CardNo, WORD axis, WORD settle_time, double err_band, WORD enable);
DMC_API short __stdcall dmc_get_pos_calibrate_config(WORD CardNo, WORD axis, WORD *settle_time, double *err_band, WORD *enable);

//文件调用
DMC_API short __stdcall dmc_userlib_loadlibrary(WORD CardNo, const char *pLibname);
DMC_API short __stdcall dmc_userlib_set_parameter(WORD CardNo, int type, const unsigned char *pParameter, int length);
DMC_API short __stdcall dmc_userlib_get_parameter(WORD CardNo, int type, unsigned char *pParameter, int length);
DMC_API short __stdcall dmc_userlib_imd_stop(WORD CardNo, WORD axis);

DMC_API short __stdcall dmc_cmp_fifo_get_fpga_receive_point(WORD CardNo, WORD cmp_no, long *receive_point);
DMC_API short __stdcall dmc_cmp_fifo_check_fpga_clear_status(WORD CardNo, WORD cmp_no, WORD *clr_status, long *clr_point);
DMC_API short __stdcall nmc_modify_slaveid(WORD CardNo, WORD index, WORD subindex, WORD newindex, const char *FileName);

DMC_API short __stdcall dmc_set_home_finish_map(WORD CardNo, WORD axis, WORD enable, WORD mode, WORD index, WORD sub_index,
                                                WORD bit_index, WORD bit_logic);

DMC_API short __stdcall dmc_get_home_finish_map(WORD CardNo, WORD axis, WORD *enable, WORD *mode, WORD *index, WORD *sub_index,
                                                WORD *bit_index, WORD *bit_logic);


DMC_API short __stdcall dmc_get_config_error_info(WORD CardNo, int *axis, int *liner, int *type, int *errorcode);

DMC_API short __stdcall dmc_set_t_pmove_extern_dectime(WORD CardNo, WORD axis, DWORD dec_time);
DMC_API short __stdcall dmc_get_t_pmove_extern_dectime(WORD CardNo, WORD axis, DWORD *dec_time);
DMC_API short __stdcall dmc_set_trajectory_splicing_error(WORD CardNo, WORD crd, double error);
DMC_API short __stdcall dmc_get_trajectory_splicing_error(WORD CardNo, WORD crd, double *error);

DMC_API short __stdcall dmc_sine_oscillate_set_cycle_num(WORD CardNo, WORD Axis, DWORD cycle_num);
DMC_API short __stdcall dmc_sine_oscillate_get_cycle_num(WORD CardNo, WORD Axis, DWORD *cycle_num);

DMC_API short __stdcall dmc_conti_wait_input_action(WORD CardNo, WORD Crd, WORD bitno, WORD on_off, double TimeOut, WORD action,
                                                    long mark);

DMC_API short __stdcall dmc_line_change_pos_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *TargetPos);

DMC_API short __stdcall dmc_arc_move_angle_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Cen_Pos,
                                                double Angle, double *Target_Pos, WORD posi_mode);
DMC_API short __stdcall dmc_arc_move_center_angle_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                       double *Cen_Pos, double Angle, WORD Arc_Dir, long Circle, WORD posi_mode);
DMC_API short __stdcall dmc_conti_line_change_pos_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *TargetPos,
                                                       int mark);

DMC_API short __stdcall nmc_sync_pmove_extern_unit(WORD CardNo, WORD AxisNum, WORD *AxisList, double *Dist, double *Max_Vel,
                                                   WORD Posimode);
DMC_API short __stdcall dmc_pvt_get_run_index(WORD CardNo, WORD axis, DWORD *index);

DMC_API short __stdcall dmc_firmware_auto_update();
DMC_API short __stdcall dmc_conti_set_blend_distance(WORD CardNo, WORD Crd, WORD Enable, double BlendDistance);

DMC_API short __stdcall nmc_set_dc_mode(WORD CardNo, WORD PortNo, WORD mode);
DMC_API short __stdcall nmc_get_dc_mode(WORD CardNo, WORD PortNo, WORD *mode);

DMC_API short __stdcall dmc_set_gear_follow_ratio(WORD CardNo, WORD axis, double ratio); //双Z轴
DMC_API short __stdcall dmc_get_gear_follow_ratio(WORD CardNo, WORD axis, double *ratio);
DMC_API short __stdcall dmc_ltc_set_outbit(WORD CardNo, WORD latch, WORD enable, WORD bitno, WORD logic, double delaytime_s,
                                           double outtime_s);
DMC_API short __stdcall dmc_ltc_get_outbit(WORD CardNo, WORD latch, WORD *enable, WORD *bitno, WORD *logic, double *delaytime_s,
                                           double *outtime_s);

DMC_API short __stdcall dmc_get_idle_crd_index(WORD CardNo);
DMC_API short __stdcall dmc_get_position_virtual(WORD CardNo, WORD axis, double *pos);


DMC_API short __stdcall dmc_check_encoder_done(WORD CardNo, WORD axis, WORD *state, double *EncoderPos);
DMC_API short __stdcall dmc_set_check_target_encoder(WORD CardNo, WORD axis, WORD TargetCheckEnable, double TargetError,
                                                     double TargetCheckTime_s);

DMC_API short __stdcall dmc_get_check_target_encoder(WORD CardNo, WORD axis, WORD *TargetCheckEnable, double *TargetError,
                                                     double *TargetCheckTime_s)

    ;
DMC_API short __stdcall dmc_set_check_inp_encoder(WORD CardNo, WORD axis, WORD InpCheckEnable, double InpError,
                                                  double InpCheckTime_s);
DMC_API short __stdcall dmc_get_check_inp_encoder(WORD CardNo, WORD axis, WORD *InpCheckEnable, double *InpError,
                                                  double *InpCheckTime_s);
DMC_API short __stdcall dmc_set_connect_to_encoder(WORD CardNo, WORD axis, WORD enable, double error);
DMC_API short __stdcall dmc_get_connect_to_encoder(WORD CardNo, WORD axis, WORD *enable, double *error);

DMC_API short __stdcall dmc_set_robot_config(WORD CardNo, WORD Crd, short robot_type, short elbow, short joint_num,
                                             short *joint_list, double *rx, double *tx, double *rz, double *tz);
DMC_API short __stdcall dmc_get_robot_config(WORD CardNo, WORD Crd, short *robot_type, short *elbow, short *joint_num,
                                             short *joint_list, double *rx, double *tx, double *rz, double *tz);
DMC_API short __stdcall dmc_set_robot_enable(WORD CardNo, WORD Crd, short user_crd, short tool_crd, short enable);
DMC_API short __stdcall dmc_robot_ptp_move(WORD CardNo, WORD Crd, short joint_num, short *joint_list, double *joint_pos);
DMC_API short __stdcall dmc_get_robot_sts(WORD CardNo, WORD Crd, short *complete, short *user_crd, short *tool_crd,
                                          short *enable);
DMC_API short __stdcall dmc_get_robot_pos(WORD CardNo, WORD Crd, double *pos);
DMC_API short __stdcall dmc_set_robot_kinematics_calib(WORD CardNo, WORD Crd, double *delta_rx, double *delta_tx,
                                                       double *delta_rz, double *delta_tz);
DMC_API short __stdcall dmc_get_robot_kinematics_calib(WORD CardNo, WORD Crd, double *delta_rx, double *delta_tx,
                                                       double *delta_rz, double *delta_tz);
DMC_API short __stdcall dmc_robot_kinematics_calib(WORD CardNo, WORD Crd, double *ja, double *jb, double *jc, double *jd,
                                                   double *je, double *jf, double *jg, double *jh, double *ji, double *delta_x,
                                                   double *delta_y, double *delta_z);
DMC_API short __stdcall dmc_set_robot_user_coordinate(WORD CardNo, WORD Crd, short user_crd, short complete, double *mat);
DMC_API short __stdcall dmc_get_robot_user_coordinate(WORD CardNo, WORD Crd, short user_crd, short *complete, double *mat);
DMC_API short __stdcall dmc_robot_user_coordinate(WORD CardNo, WORD Crd, short user_crd, double *p0, double *px, double *py);
DMC_API short __stdcall dmc_set_robot_tool_coordinate(WORD CardNo, WORD Crd, short tool_crd, short complete, double *mat);
DMC_API short __stdcall dmc_get_robot_tool_coordinate(WORD CardNo, WORD Crd, short tool_crd, short *complete, double *mat);
DMC_API short __stdcall dmc_robot_tool_coordinate(WORD CardNo, WORD Crd, short tool_crd, double *p1, double *p2, double *p3,
                                                  double *p4, double *p5, double *p6, double *p0, double *px, double *py);
DMC_API short __stdcall dmc_robot_workspace_detect(WORD CardNo, WORD Crd, double *pos);


DMC_API short __stdcall dmc_conti_set_wait_flag(WORD CardNo, WORD Crd, int mark, WORD wait_flag);
DMC_API short __stdcall dmc_conti_get_wait_flag(WORD CardNo, WORD Crd, int *mark, WORD *wait_flag);

DMC_API short __stdcall dmc_conti_set_arc_blend_enable(WORD CardNo, WORD Crd, WORD Enable);
DMC_API short __stdcall dmc_conti_get_arc_blend_enable(WORD CardNo, WORD Crd, WORD *Enable);

DMC_API short __stdcall dmc_t_pmove_extern_unit_ex(WORD CardNo, WORD axis, double MidPos, double TargetPos, double Min_Vel,
                                                   double Max_Vel, double stop_Vel, double acc, double dec, WORD posi_mode);

DMC_API short __stdcall dmc_check_success_pulse_ex(WORD CardNo, WORD axis, int delay_ms);
DMC_API short __stdcall dmc_check_success_encoder_ex(WORD CardNo, WORD axis, int delay_ms);

DMC_API short __stdcall dmc_cam_write_points_packet(WORD CardNo, WORD cam_table_id, WORD cam_point_num, double s_range_up,
                                                    double s_range_dn, double *master_pos, double *slave_pos, double *slave_vel,
                                                    double *slave_acc, double *slave_jerk, WORD *type);

DMC_API short __stdcall dmc_cam_read_points_packet(WORD CardNo, WORD cam_table_id, WORD *cam_point_num, double *s_range_up,
                                                   double *s_range_dn, double *master_pos, double *slave_pos, double *slave_vel,
                                                   double *slave_acc, double *slave_jerk, WORD *type);

DMC_API short __stdcall dmc_set_pwm_output_extern(WORD CardNo, WORD pwm, WORD enable, double width_us, double frequency,
                                                  DWORD number);

DMC_API short __stdcall dmc_spline_pmove(WORD CardNo, WORD axis, double pos, double vs, double vm, double ve, double as,
                                         double ae, double rmd_as, double rmd_ae, int num_ts, int num_tm, int num_te,
                                         double cur_as, double cur_ae, WORD posi_mode);

DMC_API short __stdcall dmc_set_plan_mode(WORD CardNo, WORD axis, WORD mode);
DMC_API short __stdcall dmc_get_plan_mode(WORD CardNo, WORD axis, WORD *mode);

DMC_API short __stdcall dmc_set_emg_lock(WORD CardNo, WORD enable, WORD bit_no, WORD level, DWORD out_mark, DWORD out_level);
DMC_API short __stdcall dmc_get_emg_lock(WORD CardNo, WORD *enable, WORD *bit_no, WORD *level, DWORD *out_mark, DWORD *out_level);
DMC_API short __stdcall dmc_emg_unlock(WORD CardNo);
DMC_API short __stdcall dmc_emg_lock_status(WORD CardNo, WORD *lock_status, WORD *lock_type);

DMC_API short __stdcall dmc_set_vector_profile_extern(WORD CardNo, WORD Crd, double Min_Vel, double Max_Vel, double Acc,
                                                      double Dec, double Ajerk, double Djerk, double stop_vel);
DMC_API short __stdcall dmc_get_vector_profile_extern(WORD CardNo, WORD Crd, double *Min_Vel, double *Max_Vel, double *Acc,
                                                      double *Dec, double *Ajerk, double *Djerk, double *stop_vel);
DMC_API short __stdcall dmc_set_vector_plan_mode(WORD CardNo, WORD Crd, WORD mode);
DMC_API short __stdcall dmc_get_vector_plan_mode(WORD CardNo, WORD Crd, WORD *mode);

DMC_API short __stdcall nmc_set_data_offset_time(WORD CardNo, int offset_us);
DMC_API short __stdcall nmc_get_data_offset_time(WORD CardNo, int *offset_us);
DMC_API short __stdcall dmc_check_done_multicoor_extern(WORD CardNo, WORD Crd, WORD *crd_state, DWORD *crd_stop_reason,
                                                        DWORD *axis_stop_reason);
DMC_API short __stdcall dmc_get_error_description(int errcocode, char *description);
DMC_API short __stdcall dmc_write_outport_mask(WORD CardNo, WORD port, DWORD mask, DWORD state, DWORD reverse_time_ms);

DMC_API short __stdcall dmc_set_pso_output_delay(WORD CardNo, WORD axis, DWORD delay_cycle);
DMC_API short __stdcall dmc_get_pso_output_delay(WORD CardNo, WORD axis, DWORD *delay_cycle);
DMC_API short __stdcall dmc_set_gap_cmp_space(WORD CardNo, WORD crd, double space);
DMC_API short __stdcall dmc_get_gap_cmp_space(WORD CardNo, WORD crd, double *space);

DMC_API short __stdcall nmc_ecat_read_slave_register(WORD CardNo, WORD wSlaveAddress, WORD wRegisterOffset, WORD wLen,
                                                     char *pdwData);
DMC_API short __stdcall nmc_ecat_write_slave_register(WORD CardNo, WORD wSlaveAddress, WORD wRegisterOffset, WORD wLen,
                                                      char *pdwData);

DMC_API short __stdcall dmc_set_inp_map_input(WORD CardNo, WORD axis, WORD enable, WORD index, WORD sub_index, WORD bit_index,
                                              WORD inp_validvalue, WORD connect2checkdone);
DMC_API short __stdcall dmc_get_inp_map_input(WORD CardNo, WORD axis, WORD *enable, WORD *index, WORD *sub_index, WORD *bit_index,
                                              WORD *inp_validvalue, WORD *connect2checkdone);
DMC_API short __stdcall dmc_get_pwm_state(WORD CardNo, WORD channel, WORD *state);

DMC_API short __stdcall dmc_rtcp_rotation_axis_transform_param(WORD CardNo, WORD axis, double rod_len);
DMC_API short __stdcall dmc_rtcp_get_rotation_axis_transform_param(WORD CardNo, WORD axis, double *rod_len);
DMC_API short __stdcall dmc_rtcp_rotation_axis_transform_enable(WORD CardNo, WORD axis, WORD enable);
DMC_API short __stdcall dmc_rtcp_get_rotation_axis_transform_enable(WORD CardNo, WORD axis, WORD *enable);
DMC_API short __stdcall dmc_circle_move_3point_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                    double *Mid_Pos, long Circle, WORD posi_mode);
DMC_API short __stdcall dmc_conti_circle_move_3point_unit(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                          double *Mid_Pos, long Circle, WORD posi_mode, long mark);

DMC_API short __stdcall dmc_get_cam_in(WORD CardNo, WORD slave_axis, WORD *master_axis, WORD *execute, WORD *conti_update,
                                       WORD *cam_table, WORD *periodic, WORD *master_abs, WORD *slave_abs, double *master_offset,
                                       double *slave_offset, double *master_scaling, double *slave_scaling,
                                       double *master_start_dist, double *master_sync_pos, double *active_pos, WORD *acvive_mode,
                                       WORD *start_mode, double *velocity, double *acc, double *dec, double *jerk,
                                       WORD *master_source, WORD *buffer_mode);
DMC_API short __stdcall dmc_mc_phasing(WORD CardNo, WORD slave_axis, WORD master_axis, WORD execute, double phase_shift,
                                       double velocity, double acc, double dec, double jerk);
DMC_API short __stdcall dmc_get_mc_phasing_status(WORD CardNo, WORD slave_axis, WORD *done, WORD *busy, WORD *cmd_aborted,
                                                  WORD *error, DWORD *error_id);
DMC_API short __stdcall dmc_get_mc_phasing(WORD CardNo, WORD slave_axis, WORD *master_axis, WORD *execute, double *phase_shift,
                                           double *velocity, double *acc, double *dec, double *jerk);

DMC_API short __stdcall dmc_set_axis_pwm_follow_speed(WORD CardNo, WORD pwm_no, WORD axis, WORD mode, double max_vel,
                                                      double min_vel, double max_value, double out_value, double min_value,
                                                      WORD min_ctl_mode);
DMC_API short __stdcall dmc_get_axis_pwm_follow_speed(WORD CardNo, WORD pwm_no, WORD *axis, WORD *mode, double *max_vel,
                                                      double *min_vel, double *max_value, double *out_value, double *min_value,
                                                      WORD *min_ctl_mode);
DMC_API short __stdcall dmc_set_axis_pwm_follow_speed_enable(WORD CardNo, WORD pwm_no, WORD enable);
DMC_API short __stdcall dmc_get_axis_pwm_follow_speed_enable(WORD CardNo, WORD pwm_no, WORD *enable);

DMC_API short __stdcall dmc_umove_unit(WORD CardNo, WORD group, WORD mode, WORD sources, WORD io_index, WORD io_value,
                                       WORD up_axis, double up_pos, double up_safe_distance, WORD move_num, WORD *move_axis_list,
                                       double *move_pos, double *move_safe_distance, WORD down_axis, double down_pos,
                                       WORD posi_mode);
DMC_API short __stdcall dmc_get_umove_runsts(WORD CardNo, WORD group, WORD *runsts);
DMC_API short __stdcall dmc_umove_stop(WORD CardNo, WORD group);

DMC_API short __stdcall dmc_set_knife_positioned(WORD CardNo, WORD Crd, double SecondVel, double SecondPos);
DMC_API short __stdcall dmc_get_knife_positioned(WORD CardNo, WORD Crd, double *SecondVel, double *SecondPos);
DMC_API short __stdcall dmc_set_knife_positioned_enable(WORD CardNo, WORD Crd, WORD enable);
DMC_API short __stdcall dmc_get_knife_positioned_enable(WORD CardNo, WORD Crd, WORD *enable);
DMC_API short __stdcall dmc_return_to_zero(WORD CardNo, WORD axis);

DMC_API short __stdcall dmc_conti_set_lookahead_path_error(WORD CardNo, WORD Crd, double patherr);
DMC_API short __stdcall dmc_conti_get_lookahead_path_error(WORD CardNo, WORD Crd, double *patherr);
DMC_API short __stdcall dmc_conti_safe_pause_list(WORD CardNo, WORD Crd, WORD safe_axis_num, WORD *safe_axis_list,
                                                  double *distance, double *vstart, double *vsteady, double *vend,
                                                  double *acc_time, double *dec_time, WORD posi_mode);
DMC_API short __stdcall dmc_set_axis_da_follow_speed(WORD CardNo, WORD da_channel, WORD axis, WORD mode, double max_vel,
                                                     double min_vel, double max_value, double min_value, double offset);
DMC_API short __stdcall dmc_get_axis_da_follow_speed(WORD CardNo, WORD da_channel, WORD *axis, WORD *mode, double *max_vel,
                                                     double *min_vel, double *max_value, double *min_value, double *offset);
DMC_API short __stdcall dmc_set_axis_da_follow_speed_enable(WORD CardNo, WORD da_channel, WORD enable);
DMC_API short __stdcall dmc_get_axis_da_follow_speed_enable(WORD CardNo, WORD da_channel, WORD *enable);
DMC_API short __stdcall dmc_conti_force_set_position(WORD CardNo, WORD Crd, WORD axis_num, WORD *axis_list, double *position);

DMC_API short __stdcall dmc_set_axis_da_follow_speed_extern(WORD CardNo, WORD da_channel, WORD axis, WORD mode, WORD segment,
                                                            double *vel, double *value);
DMC_API short __stdcall dmc_get_axis_da_follow_speed_extern(WORD CardNo, WORD da_channel, WORD *axis, WORD *mode, WORD *segment,
                                                            double *vel, double *value);

DMC_API short __stdcall dmc_conti_set_node_da_enable(WORD CardNo, WORD Crd, WORD node_id, WORD channel, WORD enable);
DMC_API short __stdcall dmc_conti_set_node_da_output(WORD CardNo, WORD Crd, WORD node_id, WORD channel, double Vout);
DMC_API short __stdcall dmc_conti_gantry_move(WORD CardNo, WORD Crd, WORD master_axis, WORD slave_num, WORD *slave_axis_list,
                                              WORD on_off);
DMC_API short __stdcall dmc_conti_set_gantry_error_protect_unit(WORD CardNo, WORD Crd, WORD master_axis, double dstp_err,
                                                                double emg_err, WORD on_off);

DMC_API short __stdcall dmc_m_add_sigaxis_moveseg_data_ex(WORD CardNo, WORD group, WORD Axis, double Target_pos, DWORD mark);
DMC_API short __stdcall dmc_m_add_wait_event_data(WORD CardNo, WORD group, WORD event, WORD num, WORD CompareOperator,
                                                  double target_value, DWORD mark);
DMC_API short __stdcall dmc_m_add_trigger_data(WORD CardNo, WORD group, WORD mode, WORD num, double Target_Value, DWORD mark);
DMC_API short __stdcall dmc_m_add_time_delay(WORD CardNo, WORD group, double Time_delay, DWORD mark);

DMC_API short __stdcall dmc_set_feedforward_profile(WORD CardNo, WORD Axis, double vel_offset_coef, double tor_offset_coef);
DMC_API short __stdcall dmc_get_feedforward_profile(WORD CardNo, WORD Axis, double *vel_offset_coef, double *tor_offset_coef);


///////////
//其他头文件待更新
//////////

DMC_API short __stdcall dmc_set_modulo_profile(WORD CardNo, WORD axis, WORD enable, double Modulo_Vel);
DMC_API short __stdcall dmc_get_modulo_profile(WORD CardNo, WORD axis, WORD *enable, double *Modulo_Vel);

DMC_API short __stdcall dmc_line_mutli_line(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, WORD PointNum, double (*pos)[8],
                                            WORD wait_mark, WORD wait_enable, WORD posi_mode);

DMC_API short __stdcall dmc_conti_line_unit_G0(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *pPosList,
                                               WORD posi_mode, int mark);
DMC_API short __stdcall dmc_conti_arc_move_center_unit_G0(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                          double *Cen_Pos, WORD Arc_Dir, int Circle, WORD posi_mode, int mark);
DMC_API short __stdcall dmc_conti_arc_move_radius_unit_G0(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList, double *Target_Pos,
                                                          WORD Arc_Radius, WORD Arc_Dir, int Circle, WORD posi_mode, int mark);
DMC_API short __stdcall dmc_conti_arc_move_3points_unit_G0(WORD CardNo, WORD Crd, WORD AxisNum, WORD *AxisList,
                                                           double *Target_Pos, double *Mid_Pos, int Circle, WORD posi_mode,
                                                           int mark);

//总线卡新增读取所有IO状态功能
DMC_API short __stdcall dmc_read_outport_array(WORD CardNo, uint32 portNum, uint32 *status);
DMC_API short __stdcall dmc_read_inport_array(WORD CardNo, uint32 portNum, uint32 *status);

//设置/读取指令缓存运动标志位状态
DMC_API short __stdcall dmc_m_set_wait_flag(WORD CardNo, WORD Group, WORD FlagNo, WORD Wait_Flag);
DMC_API short __stdcall dmc_m_get_wait_flag(WORD CardNo, WORD Group, WORD FlagNo, WORD *Wait_Flag);

//反向间隙功能扩展，支持多种补偿模式
DMC_API short __stdcall dmc_set_backlash_unit_extern(WORD CardNo, WORD axis, double backlash, WORD mode, short dir, double vel,
                                                     double acc, WORD time_ms);
DMC_API short __stdcall dmc_get_backlash_unit_extern(WORD CardNo, WORD axis, double *backlash, WORD *mode, short *dir,
                                                     double *vel, double *acc, WORD *time_ms);

//无需映射直接使用PDO的方式操作对象字典功能
DMC_API short __stdcall nmc_write_rxpdo(WORD CardNo, WORD portnum, WORD slave_station_addr, WORD index, WORD subindex,
                                        WORD bitlength, BYTE *data);
DMC_API short __stdcall nmc_read_rxpdo(WORD CardNo, WORD portnum, WORD slave_station_addr, WORD index, WORD subindex,
                                       WORD bitlength, BYTE *data);
DMC_API short __stdcall nmc_read_txpdo(WORD CardNo, WORD portnum, WORD slave_station_addr, WORD index, WORD subindex,
                                       WORD bitlength, BYTE *data);

DMC_API short __stdcall dmc_syncmotion_set_enable(WORD CardNo, WORD slaveAxisNo, WORD enable);
DMC_API short __stdcall dmc_syncmotion_get_enable(WORD CardNo, WORD slaveAxisNo, WORD *enable);
DMC_API short __stdcall dmc_set_syncmotion_configparams(WORD CardNo, WORD masterAxisNo, WORD slaveAxisNo, WORD follow_src_sel,
                                                        WORD master_type, double scale_coe, WORD dir_rev);
DMC_API short __stdcall dmc_get_syncmotion_configparams(WORD CardNo, WORD slaveAxisNo, WORD *masterAxisNo, WORD *follow_src_sel,
                                                        WORD *master_type, double *scale_coe, WORD *dir_rev);
DMC_API short __stdcall dmc_syncmotion_cancle(WORD CardNo, WORD slaveAxisNo, double dec, double jerk);
DMC_API short __stdcall dmc_syncmotion_updatescale(WORD CardNo, WORD slaveAxisNo, double scale_coe);

//设置 alm 信号特殊控制功能：可实现报警后断开使能
DMC_API short __stdcall dmc_set_alm_control_function(WORD CardNo, WORD axis, WORD control_function);
DMC_API short __stdcall dmc_get_alm_control_function(WORD CardNo, WORD axis, WORD *control_function);


DMC_API short __stdcall dmc_calculate_axis_plan_time(WORD CardNo, double Dis, double Start_Vel, double Max_Vel, double End_Vel,
                                                     double Tacc, double Tdec, double sTime, double *Tsum);

//二维位置补偿功能
DMC_API short __stdcall dmc_set_leadscrew_comp_2D_config_unit(WORD CardNo, WORD axis, WORD *ref_axis, double *ref_axis_start_pos,
                                                              double *ref_axis_length, WORD *ref_axis_segment, double *Compos);
DMC_API short __stdcall dmc_get_leadscrew_comp_2D_config_unit(WORD CardNo, WORD axis, WORD *ref_axis, double *ref_axis_start_pos,
                                                              double *ref_axis_length, WORD *ref_axis_segment, double *Compos);
DMC_API short __stdcall dmc_set_leadscrew_comp_2D_angle_unit(WORD CardNo, WORD axis, WORD *ref_axis, double *ref_axis_start_pos,
                                                             double *ref_axis_length, double angle);
DMC_API short __stdcall dmc_get_leadscrew_comp_2D_angle_unit(WORD CardNo, WORD axis, WORD *ref_axis, double *ref_axis_start_pos,
                                                             double *ref_axis_length, double *angle);
DMC_API short __stdcall dmc_set_leadscrew_comp_2D_enable(WORD CardNo, WORD axis, WORD mode, WORD enable);
DMC_API short __stdcall dmc_get_leadscrew_comp_2D_enable(WORD CardNo, WORD axis, WORD *mode, WORD *enable);

//轨迹算法升级连续插补新增功能：包括减速角停止角、拐弯时间、小圆限速、衔接点速度规划模式设置等功能
DMC_API short __stdcall dmc_conti_set_coordinate_params(WORD CardNo, WORD Crd, double T, double radius, double limit_vel);
DMC_API short __stdcall dmc_conti_get_coordinate_params(WORD CardNo, WORD Crd, double *T, double *radius, double *limit_vel);
DMC_API short __stdcall dmc_conti_set_corner_angle_param(WORD CardNo, WORD Crd, double dec_angle, double stop_angle, WORD enable);
DMC_API short __stdcall dmc_conti_get_corner_angle_param(WORD CardNo, WORD Crd, double *dec_angle, double *stop_angle,
                                                         WORD *enable);
DMC_API short __stdcall dmc_conti_set_transvelocity_mode(WORD CardNo, WORD Crd, WORD transvel_mode);
DMC_API short __stdcall dmc_conti_get_transvelocity_mode(WORD CardNo, WORD Crd, WORD *transvel_mode);


DMC_API short __stdcall dmc_send_pack(WORD CardNo, WORD mode, WORD length, char *pBuf);
DMC_API short __stdcall dmc_conti_delay_set_mode(WORD CardNo, WORD Crd, WORD mode);
DMC_API short __stdcall dmc_conti_delay_get_mode(WORD CardNo, WORD Crd, WORD *mode);

//物理节点使能功能
DMC_API short __stdcall nmc_set_alias_address_enable(WORD CardNo, WORD enable);
DMC_API short __stdcall nmc_get_alias_address_enable(WORD CardNo, WORD *enable, WORD *states);

DMC_API short __stdcall dmc_get_system_version(WORD CardNo, char *SystemVersion);

DMC_API short __stdcall dmc_set_track_config_unit(WORD CardNo, WORD m_slave_axis_num, WORD *m_master_axis, WORD *m_slave_axis,
                                                  WORD *m_start_distance, WORD *m_coordinate_axis, double *m_angle,
                                                  double *m_master_vel, double *m_start_time, double *m_finish_time,
                                                  double *m_sync_start_pos, double *m_sync_end_pos, double *m_finish_pos);
DMC_API short __stdcall dmc_get_track_config_unit(WORD CardNo, WORD slave_axis, WORD *m_track_state);
DMC_API short __stdcall dmc_add_move_config_unit(WORD CardNo, WORD add_axis, WORD added_axis, WORD enable);

DMC_API short __stdcall nmc_get_same_alias_address_slaves(WORD CardNo, WORD *SlaveNum, WORD *SlaveList);

DMC_API short __stdcall dmc_input_shaper_on(WORD CardNo, WORD axis, double *cnvA, DWORD *cnvT, WORD num);
DMC_API short __stdcall dmc_input_shaper_off(WORD CardNo, WORD axis);
DMC_API short __stdcall dmc_get_input_shaper_status(WORD CardNo, WORD axis, WORD *status);
DMC_API short __stdcall dmc_set_input_shaper_compesation_enable(WORD CardNo, WORD axis, WORD enable);
DMC_API short __stdcall dmc_get_input_shaper_compesation_enable(WORD CardNo, WORD axis, WORD *enable);
DMC_API short __stdcall dmc_set_input_shaper_compesation(WORD CardNo, WORD axis, double T, double freq, double zeta,
                                                         double alpha);
DMC_API short __stdcall dmc_get_input_shaper_compesation(WORD CardNo, WORD axis, double *T, double *freq, double *zeta,
                                                         double *alpha);
DMC_API short __stdcall dmc_set_input_shaper_compesation_error(WORD CardNo, WORD axis, double delay_T, double pos_error);
DMC_API short __stdcall dmc_get_input_shaper_compesation_error(WORD CardNo, WORD axis, double *delay_T, double *pos_error);

DMC_API short __stdcall dmc_m_add_sigaxis_moveseg_data_multi(WORD CardNo, WORD group, WORD AxisNum, WORD *AxisList,
                                                             double *Target_pos, uint32 *mark);
DMC_API short __stdcall dmc_set_func_enable(WORD CardNo, WORD mode, WORD enable);
DMC_API short __stdcall dmc_get_func_enable(WORD CardNo, WORD mode, WORD *enable);

//总线回零参数设置支持加速度模式
DMC_API short __stdcall nmc_set_home_profile_acc(WORD CardNo, WORD axis, WORD home_mode, double Low_Vel, double High_Vel,
                                                 double Tacc, double Tdec, double offsetpos);
DMC_API short __stdcall nmc_get_home_profile_acc(WORD CardNo, WORD axis, WORD *home_mode, double *Low_Vel, double *High_Vel,
                                                 double *Tacc, double *Tdec, double *Offsetpos);

DMC_API short __stdcall dmc_get_ad_input_append(WORD CardNo, WORD Channel, double *Vout);

//PWM开始优先输出电平部分设置功能
DMC_API short __stdcall dmc_conti_set_pwm_output_extern(WORD CardNo, WORD Crd, WORD pwm_no, WORD enable, double width_us,
                                                        double frequency, uint32 number);
DMC_API short __stdcall dmc_set_pwm_mode(WORD CardNo, WORD pwm_no, WORD startmode, WORD stopmode);
DMC_API short __stdcall dmc_get_pwm_mode(WORD CardNo, WORD pwm_no, WORD *startmode, WORD *stopmode);

//连续插补单轴运动支持速度缓存设置功能
DMC_API short __stdcall dmc_conti_set_profile_unit(WORD CardNo, WORD Crd, double Min_Vel, double Max_vel, double Tacc,
                                                   double Tdec, double Stop_Vel);
DMC_API short __stdcall dmc_conti_pmove_extern_unit(WORD CardNo, WORD Crd, WORD axis, double dist, double Min_Vel, double Max_Vel,
                                                    double Tacc, double Tdec, double Stop_Vel, WORD posi_mode, WORD mode,
                                                    int mark);

//连续插补批量添加功能
DMC_API short __stdcall dmc_conti_pack_on(WORD CardNo);
DMC_API short __stdcall dmc_conti_pack_off(WORD CardNo);
DMC_API short __stdcall dmc_conti_pack_flush(WORD CardNo);

//
DMC_API short __stdcall dmc_sine_oscillate_extern(WORD CardNo, WORD Axis, double Amplitude, double Frequency, double cycle,
                                                  WORD param);
DMC_API short __stdcall dmc_sine_oscillate_extern_unit(WORD CardNo, WORD Axis, double Amplitude, double Frequency, double cycle,
                                                       WORD param);

DMC_API short __stdcall dmc_write_outport_array(WORD CardNo, uint32 portNum, uint32 *status);

//连续插补中缓存操作SDO/PDO功能
DMC_API short __stdcall dmc_conti_set_node_od(WORD CardNo, WORD Crd, WORD NodeNum, WORD Index, WORD SubIndex, WORD ValLength,
                                              uint32 Value);
DMC_API short __stdcall dmc_conti_set_rxpdo_extra(WORD CardNo, WORD Crd, WORD Address, WORD DataLen, WORD Mode, WORD ModeVal,
                                                  uint32 Value);

//指令缓存中新增操作SDO/PDO功能
DMC_API short __stdcall dmc_m_add_trigger_set_od(WORD CardNo, WORD Group, WORD NodeNum, WORD Index, WORD SubIndex, WORD ValLength,
                                                 uint32 Value, uint32 Mark);
DMC_API short __stdcall dmc_m_add_trigger_set_rxpdo_extra(WORD CardNo, WORD Group, WORD Address, WORD DataLen, WORD Mode,
                                                          WORD ModeVal, uint32 Value, uint32 Mark);

//插补单轴限速使能
DMC_API short __stdcall dmc_set_profile_limit_by_axis(WORD CardNo, WORD axis, WORD enable);
DMC_API short __stdcall dmc_get_profile_limit_by_axis(WORD CardNo, WORD axis, WORD *enable);

//总线卡二维螺距补偿功能
DMC_API short __stdcall dmc_set_leadscrew_comp_2D_config_unit_ex(WORD CardNo, WORD table_index, WORD axis, WORD *ref_axis,
                                                                 double *ref_axis_start_pos, double *ref_axis_length,
                                                                 WORD *ref_axis_segment, double *value);
DMC_API short __stdcall dmc_get_leadscrew_comp_2D_config_unit_ex(WORD CardNo, WORD table_index, WORD *axis, WORD *ref_axis,
                                                                 double *ref_axis_start_pos, double *ref_axis_length,
                                                                 WORD *ref_axis_segment, double *value);
DMC_API short __stdcall dmc_set_leadscrew_comp_2D_angle_unit_ex(WORD CardNo, WORD table_index, WORD comp_axis, WORD *ref_axis,
                                                                double *ref_axis_start_pos, double *ref_axis_length,
                                                                double angle);
DMC_API short __stdcall dmc_get_leadscrew_comp_2D_angle_unit_ex(WORD CardNo, WORD *table_index, WORD *axis, WORD *ref_axis,
                                                                double *ref_axis_start_pos, double *ref_axis_length,
                                                                double *angle);
DMC_API short __stdcall dmc_set_leadscrew_comp_2D_enable_ex(WORD CardNo, WORD table_index, WORD mode, WORD enable);
DMC_API short __stdcall dmc_get_leadscrew_comp_2D_enable_ex(WORD CardNo, WORD table_index, WORD *mode, WORD *enable);

//EMC系列控制器ip地址设置和获取功能
DMC_API short __stdcall dmc_set_ipaddr(WORD CardNo, const char *IpAddr);
DMC_API short __stdcall dmc_get_ipaddr(WORD CardNo, char *IpAddr);

//IO翻转动作清除功能
DMC_API short __stdcall dmc_clear_io_action(WORD CardNo, WORD Crd, WORD IoMask, WORD Mode);

DMC_API short __stdcall dmc_m_remain_space(WORD CardNo, WORD Crd, uint32 *data);

DMC_API short __stdcall dmc_conti_set_trans_arc_speed_mode(WORD CardNo, WORD Crd, WORD mode);
DMC_API short __stdcall dmc_conti_get_trans_arc_speed_mode(WORD CardNo, WORD Crd, WORD *mode);

//5X10连续插补中 CAN 模块 DA 速度跟随功能
DMC_API short __stdcall dmc_conti_set_node_da_follow_speed(WORD CardNo, WORD Crd, WORD node_id, WORD da_no, double MaxVel,
                                                           double MaxValue, double acc_offset, double dec_offset, double acc_dist,
                                                           double dec_dist);
DMC_API short __stdcall dmc_conti_get_node_da_follow_speed(WORD CardNo, WORD Crd, WORD node_id, WORD da_no, double *MaxVel,
                                                           double *MaxValue, double *acc_offset, double *dec_offset,
                                                           double *acc_dist, double *dec_dist);

DMC_API short __stdcall dmc_m_set_factor_error(WORD CardNo, WORD axis, WORD enable, double Positive_error, double Negative_error,
                                               WORD retain);

DMC_API short __stdcall dmc_rAxis_comp_config(WORD CardNo, short axis, short enable, int cnt, double *angle, double *xPos,
                                              double *yPos);
DMC_API short __stdcall dmc_rAxis_comp_loop(WORD CardNo, short axis, short mode, double angle, double xPos, double yPos,
                                            double *new_xPos, double *new_yPos);

//点位运动Pro规划模式功能
DMC_API short __stdcall dmc_pmove_pro_unit(WORD CardNo, WORD axis_no, double pos, WORD pos_mode, WORD plan_mode,
                                           unsigned char *vel_param, unsigned char *plan_result);

//5X10系列连续插补中相对于轨迹段终点 DA 提前/滞后跟随输出
DMC_API short __stdcall dmc_conti_delay_node_da_follow_to_start(WORD CardNo, WORD Crd, WORD node_id, WORD da_no, WORD delay_mode,
                                                                double delay_value, double min_value, double max_value,
                                                                double period);
DMC_API short __stdcall dmc_conti_ahead_node_da_follow_to_stop(WORD CardNo, WORD Crd, WORD node_id, WORD da_no, WORD ahead_mode,
                                                               double ahead_value, double min_value, double max_value,
                                                               double period);

DMC_API short __stdcall dmc_printall_time();
DMC_API short __stdcall dmc_clear_time();

DMC_API short __stdcall dmc_get_pmove_consumed_time(WORD CardNo, WORD *time_receive, WORD *time_process, WORD *time_send);

//控制卡运动参数设置加速度模式接口
DMC_API short __stdcall dmc_t_pmove_extern_unit_acc(WORD CardNo, WORD axis, double MidPos, double TargetPos, double Min_Vel,
                                                    double Max_Vel, double stop_Vel, double acc, double dec, WORD posi_mode);
DMC_API short __stdcall dmc_t_pmove_extern_acc(WORD CardNo, WORD axis, double MidPos, double TargetPos, double Min_Vel,
                                               double Max_Vel, double stop_Vel, double acc, double dec, WORD posi_mode);
DMC_API short __stdcall dmc_t_pmove_extern_softstart_unit_acc(WORD CardNo, WORD axis, double MidPos, double TargetPos,
                                                              double start_Vel, double Max_Vel, double stop_Vel, DWORD delay_ms,
                                                              double Max_Vel2, double stop_vel2, double acc_time, double dec_time,
                                                              WORD posi_mode);
DMC_API short __stdcall dmc_t_pmove_extern_softstart_acc(WORD CardNo, WORD axis, double MidPos, double TargetPos,
                                                         double start_Vel, double Max_Vel, double stop_Vel, DWORD delay_ms,
                                                         double Max_Vel2, double stop_vel2, double acc_time, double dec_time,
                                                         WORD posi_mode);
DMC_API short __stdcall dmc_change_speed_unit_acc(WORD CardNo, WORD axis, double New_Vel, double Taccdec);
DMC_API short __stdcall dmc_change_speed_acc(WORD CardNo, WORD axis, double Curr_Vel, double Taccdec);
DMC_API short __stdcall dmc_m_set_profile_unit_acc(WORD CardNo, WORD group, WORD axis, double start_vel, double max_vel,
                                                   double tacc, double tdec, double stop_vel);

//5X10系列连续插补 CAN模块DA 跟随功能
DMC_API short __stdcall dmc_set_node_da_follow_speed_extern(WORD CardNo, WORD node_id, WORD da_channel, WORD axis, WORD mode,
                                                            WORD segment, double *vel, double *value);
DMC_API short __stdcall dmc_get_node_da_follow_speed_extern(WORD CardNo, WORD node_id, WORD da_channel, WORD *axis, WORD *mode,
                                                            WORD *segment, double *vel, double *value);
DMC_API short __stdcall dmc_set_node_da_follow_speed_enable(WORD CardNo, WORD node_id, WORD da_channel, WORD enable);
DMC_API short __stdcall dmc_get_node_da_follow_speed_enable(WORD CardNo, WORD node_id, WORD da_channel, WORD *enable);


DMC_API short __stdcall nmc_set_clear_fieldbus_state_on_soft_reset(WORD CardNo, WORD enable);
DMC_API short __stdcall nmc_get_clear_fieldbus_state_on_soft_reset(WORD CardNo, WORD *enable);

DMC_API short __stdcall nmc_get_master_state(WORD CardNo, uint32 *States);


//////////
//20240522更新
/////////

//环网冗余相关状态
DMC_API short __stdcall nmc_get_cable_redundancy_enable(WORD CardNo, WORD *Enable, WORD *ErrStop);
DMC_API short __stdcall nmc_get_num_connected_slaves_red(WORD CardNo, uint32 *BrkMainSlaves, uint32 *BrkRedSlaves,
                                                         uint32 *CurMainSlaves, uint32 *CurRedSlaves);

DMC_API short __stdcall get_bar2_value(WORD CardNo, WORD offset, WORD length, uint32 *value);

//获取编码器速度功能
DMC_API short __stdcall dmc_get_encoder_speed(WORD CardNo, WORD axis, double *vel);

typedef struct
{
    float fl_val;   //pwm随动值（因变量）
    float ctrl_val; //控制值（自变量）
} PwmCurve_CtrlPoint;

typedef struct
{
    float vol_val;  //da值（因变量）
    float ctrl_val; //控制值（自变量）
} DaCurve_CtrlPoint;

//5X10系列连续插补DA--T/P跟随输出功能
DMC_API short __stdcall dmc_set_da_curve(WORD CardNo, WORD Crd, WORD curve_no, WORD ctrl_mode, WORD ctrl_point_num,
                                         DaCurve_CtrlPoint *ctrl_point);
DMC_API short __stdcall dmc_get_da_curve(WORD CardNo, WORD Crd, WORD curve_no, WORD *ctrl_mode, WORD *ctrl_point_num,
                                         DaCurve_CtrlPoint *ctrl_point);
DMC_API short __stdcall dmc_conti_da_curve_control_to_start(WORD CardNo, WORD Crd, WORD curve_no, WORD da_channel,
                                                            WORD delay_mode, float delay_value, uint32 mark);
DMC_API short __stdcall dmc_conti_da_curve_control_to_stop(WORD CardNo, WORD Crd, WORD curve_no, WORD da_channel, WORD ahead_mode,
                                                           float ahead_value, uint32 mark);
DMC_API short __stdcall dmc_conti_da_curve_control(WORD CardNo, WORD Crd, WORD curve_no, WORD on_off, WORD da_channel,
                                                   uint32 us_delay_time, uint32 mark);

DMC_API short __stdcall dmc_cmd_buf_set_axis_profile_ext(WORD CardNo, WORD group, WORD axis_num, WORD *axis_list, WORD *plan_mode,
                                                         double *start_vel, double *max_vel, double *stop_vel, double *tacc,
                                                         double *tdec, double *ja, double *jd);

DMC_API short __stdcall dmc_set_node_da_output(WORD CardNo, WORD PortNum, WORD NodeNum, WORD Group, WORD *Target_Time,
                                               WORD *Target_Voltage);
DMC_API short __stdcall dmc_set_node_DA_configure(WORD CardNo, WORD PortNum, WORD NodeNum, WORD Channel, WORD Group);
DMC_API short __stdcall dmc_set_node_DA_enable(WORD CardNo, WORD PortNum, WORD NodeNum, WORD Channel, WORD DelayPeriod);
DMC_API short __stdcall dmc_set_node_DA_stop(WORD CardNo, WORD PortNum, WORD NodeNum, WORD Channel);
DMC_API short __stdcall dmc_set_node_DA_mode(WORD CardNo, WORD Mode, WORD Address, WORD DataLen, WORD Value, WORD DelayPeriod);


DMC_API short __stdcall dmc_set_acc_jerkratio_profile_unit(WORD CardNo, WORD axis, double Min_Vel, double Max_Vel,
                                                           double averageAcc, double averageDec, double jerkAccRatio,
                                                           double jerkDecRatio, double stop_vel);
DMC_API short __stdcall dmc_get_acc_jerkratio_profile_unit(WORD CardNo, WORD axis, double *Min_Vel, double *Max_Vel,
                                                           double *averageAcc, double *averageDec, double *jerkAccRatio,
                                                           double *jerkDecRatio, double *stop_vel);
DMC_API short __stdcall dmc_set_acctime_jerkratio_profile_unit(WORD CardNo, WORD axis, double Min_Vel, double Max_Vel,
                                                               double accTime, double decTime, double jerkAccRatio,
                                                               double jerkDecRatio, double stop_vel);
DMC_API short __stdcall dmc_get_acctime_jerkratio_profile_unit(WORD CardNo, WORD axis, double *Min_Vel, double *Max_Vel,
                                                               double *accTime, double *dectime, double *jerkAccRatio,
                                                               double *jerkDecRatio, double *stop_vel);
DMC_API short __stdcall dmc_set_vector_acc_jerkratio_profile_unit(WORD CardNo, WORD CrdID, double Min_Vel, double Max_Vel,
                                                                  double averageAcc, double averageDec, double jerkAccRatio,
                                                                  double jerkDecRatio, double stop_vel);
DMC_API short __stdcall dmc_get_vector_acc_jerkratio_profile_unit(WORD CardNo, WORD CrdID, double *Min_Vel, double *Max_Vel,
                                                                  double *averageAcc, double *averageDec, double *jerkAccRatio,
                                                                  double *jerkDecRatio, double *stop_vel);
DMC_API short __stdcall dmc_set_vector_acctime_jerkratio_profile_unit(WORD CardNo, WORD axis, double Min_Vel, double Max_Vel,
                                                                      double accTime, double decTime, double jerkAccRatio,
                                                                      double jerkDecRatio, double stop_vel);
DMC_API short __stdcall dmc_get_vector_acctime_jerkratio_profile_unit(WORD CardNo, WORD CrdID, double *Min_Vel, double *Max_Vel,
                                                                      double *accTime, double *decTime, double *jerkAccRatio,
                                                                      double *jerkDecRatio, double *stop_vel);

DMC_API short __stdcall dmc_measure_time_enable(WORD CardNo, WORD enable);

DMC_API short __stdcall dmc_m_cmd_buf_set_profile_extern(WORD CardNo, WORD group, WORD axis_num, WORD *axis_list, WORD *plan_mode,
                                                         double *start_vel, double *max_vel, double *stop_vel, double *tacc,
                                                         double *tdec, double *ja, double *jd);

//脉冲卡回零功能扩展：回零原点和EZ信号支持设置寻找距离
DMC_API short __stdcall dmc_set_home_search_limit(WORD CardNo, WORD axis, WORD enable, WORD stop_mode, uint32 limit_length,
                                                  uint32 org_length, uint32 ez_length);
DMC_API short __stdcall dmc_set_home_search_limit_unit(WORD CardNo, WORD axis, WORD enable, WORD stop_mode, double limit_length,
                                                       double org_length, double ez_length);

DMC_API short __stdcall dmc_get_traj_algorithm_version(WORD CardNo, char *trajAlgorithmVersion);

//总线卡适配ISMC驱动器软着陆定制功能
DMC_API short __stdcall dmc_set_pt_soft_land_profile(WORD CardNo, WORD AxisNo, int32 PpModePos, int32 ChangePos,
                                                     uint32 PosTorLimit, uint32 DelayTime, uint32 KeepTorque, uint32 Mode);
DMC_API short __stdcall dmc_set_pt_soft_land_init_profile(WORD CardNo, WORD AxisNo, uint32 PpModeVel, uint32 PpModeAcc,
                                                          uint32 PpModeDec, int32 SuccessMaxVel, int32 ReturnPos, int32 PvModeVel,
                                                          uint32 PvModeSecondVel);
DMC_API short __stdcall dmc_get_pt_soft_land_arrive_flag(WORD CardNo, WORD AxisNo, WORD *Flag);
DMC_API short __stdcall dmc_start_pt_soft_land(WORD CardNo, WORD AxisNo, WORD RunMode);
DMC_API short __stdcall dmc_pt_soft_land_return(WORD CardNo, WORD AxisNo, WORD RunMode);
DMC_API short __stdcall dmc_stop_pt_soft_land(WORD CardNo, WORD AxisNo, WORD RunMode);
DMC_API short __stdcall dmc_clear_pt_soft_land_errcode(WORD CardNo, WORD AxisNo, WORD RunMode);
DMC_API short __stdcall dmc_set_pt_soft_land_PosTorLimit(WORD CardNo, WORD AxisNo, uint32 PosTorLimit);

DMC_API short __stdcall dmc_set_traj_min_travel_time(WORD CardNo, WORD Crd, double min_travel_time);
DMC_API short __stdcall dmc_get_traj_min_travel_time(WORD CardNo, WORD Crd, double *min_travel_time);

DMC_API short __stdcall dmc_conti_set_follow_params(WORD CardNo, WORD Crd, WORD Follow_type, WORD Master, WORD Slave);
DMC_API short __stdcall dmc_conti_get_follow_params(WORD CardNo, WORD Crd, WORD *Follow_type, WORD *Master, WORD *Slave);

DMC_API short __stdcall nmc_get_errcode_ex(WORD CardNo, WORD Channel, uint32 *ErrType, uint32 *ErrCode);
DMC_API short __stdcall nmc_fieldbus_state_get_record_info(WORD CardNo, WORD Channel, uint32 ReadErrNum, uint32 ReadErrIndex,
                                                           uint32 *TotalErrNum, WORD *ReturnErrNum, uint64 *TimeStamp,
                                                           uint32 *ErrIndex, WORD *SlaveAddr, uint32 *ErrType, uint32 *ErrCode,
                                                           uint32 *Info_0, uint32 *Info_1, uint32 *Info_2, uint32 *Info_3);
DMC_API short __stdcall nmc_fieldbus_state_set_record_stop(WORD CardNo, WORD Stop);
DMC_API short __stdcall nmc_fieldbus_state_get_record_stop(WORD CardNo, WORD *Stop);
DMC_API short __stdcall nmc_fieldbus_state_set_record_mode(WORD CardNo, WORD Mode);
DMC_API short __stdcall nmc_fieldbus_state_get_record_mode(WORD CardNo, WORD *Mode);
DMC_API short __stdcall nmc_set_system_time(WORD CardNo, WORD Year, WORD Mon, WORD Day, WORD Hour, WORD Min, WORD Sec);
DMC_API short __stdcall nmc_get_system_time(WORD CardNo, WORD *Year, WORD *Mon, WORD *Day, WORD *Hour, WORD *Min, WORD *Sec);
DMC_API short __stdcall nmc_set_master_state(WORD CardNo, uint32 States);
DMC_API short __stdcall nmc_get_master_diagnosis_info(WORD CardNo, uint32 *NumSlavesFound, uint32 *NumDCSlavesFound,
                                                      uint32 *NumCfgSlaves, uint32 *NumMbxSlaves, uint32 *TXFrames,
                                                      uint32 *RXFrames, uint32 *LostFrames, uint32 *CyclicFrames,
                                                      uint32 *CyclicLostFrames, uint32 *AcyclicFrames, uint32 *AcyclicLostFrames);
DMC_API short __stdcall nmc_clear_master_diagnosis_info(WORD CardNo, WORD Channel);
DMC_API short __stdcall nmc_set_slave_port_state(WORD CardNo, WORD SlaveId, WORD Port, WORD State);
DMC_API short __stdcall nmc_get_slave_port_state(WORD CardNo, WORD SlaveId, WORD *PortState);
DMC_API short __stdcall nmc_get_exclude_slave_list(WORD CardNo, WORD *SlaveNum, WORD *SlaveList);
DMC_API short __stdcall nmc_hot_connect_accept_topo_change(WORD CardNo, WORD Channel);
DMC_API short __stdcall nmc_hot_connect_get_mode(WORD CardNo, uint32 *Mode, WORD *BoardClose, WORD *ErrStop);
DMC_API short __stdcall nmc_hot_connect_get_group_num(WORD CardNo, uint32 *GroupNum);
DMC_API short __stdcall nmc_hot_connect_get_group_present(WORD CardNo, uint32 GroupIndex, WORD *Present);
DMC_API short __stdcall nmc_hot_connect_get_all_group_present(WORD CardNo, uint32 *GroupNum, uint32 *PresentList);
DMC_API short __stdcall nmc_hot_connect_get_slave_group(WORD CardNo, WORD NodeId, uint32 *GroupIndex);
DMC_API short __stdcall nmc_hot_connect_get_group_slaves(WORD CardNo, uint32 GroupIndex, WORD *SlaveNum, WORD *SlaveList);
DMC_API short __stdcall nmc_foe_download_file(WORD CardNo, uint32 SlaveId, uint32 Password, uint32 Timeout, const char *pFileName,
                                              const char *pFileNameControl);
DMC_API short __stdcall nmc_foe_upload_file(WORD CardNo, uint32 SlaveId, uint32 Password, uint32 Timeout, const char *pFileName,
                                            const char *pFileNameControl);

//看门狗功能扩展：支持局部输出口复位设置
DMC_API short __stdcall dmc_set_watchdog_write_outport_mask(WORD CardNo, uint32 PortNum, uint32 PortValue);
DMC_API short __stdcall dmc_get_watchdog_write_outport_mask(WORD CardNo, uint32 PortNum, uint32 *PortValue);
DMC_API short __stdcall dmc_set_watchdog_write_outport_mask_ex(WORD CardNo, uint32 PortNum, uint32 PortValue);
DMC_API short __stdcall dmc_get_watchdog_write_outport_mask_ex(WORD CardNo, uint32 PortNum, uint32 *PortValue);
DMC_API short __stdcall dmc_set_watchdog_outport_mask_enable(WORD CardNo, uint32 PortNum, uint32 Enable);
DMC_API short __stdcall dmc_get_watchdog_outport_mask_enable(WORD CardNo, uint32 PortNum, uint32 *Enable);

//IO计数功能扩展：支持按照IO节点号进行设置计数
DMC_API short __stdcall nmc_set_io_count_mode(WORD CardNo, WORD PortNum, WORD NodeID, WORD BitNo, WORD Mode, WORD FilterTime);
DMC_API short __stdcall nmc_get_io_count_mode(WORD CardNo, WORD PortNum, WORD NodeID, WORD BitNo, WORD *Mode, WORD *FilterTime);
DMC_API short __stdcall nmc_set_io_count_value(WORD CardNo, WORD PortNum, WORD NodeID, WORD BitNo, WORD CountValue);
DMC_API short __stdcall nmc_get_io_count_value(WORD CardNo, WORD PortNum, WORD NodeID, WORD BitNo, WORD *CountValue);

DMC_API short __stdcall nmc_get_exclude_slave_enable(WORD CardNo, WORD *Enable);

DMC_API short __stdcall dmc_m_add_line_moveseg_data_multi(WORD CardNo, WORD group, WORD AxisNum, WORD *AxisList,
                                                          double *Target_pos, long mark);

DMC_API short __stdcall dmc_cmd_buf_set_line_profile_ext(WORD CardNo, WORD group, WORD plan_mode, double start_vel,
                                                         double max_vel, double stop_vel, double tacc, double tdec, double ja,
                                                         double jd);


#ifdef __cplusplus
}
#endif

#endif
