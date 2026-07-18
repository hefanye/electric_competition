#ifndef __UI_DEFINES_H
#define __UI_DEFINES_H

/*=============================================================================
 * 页面名称 (对应 DGUS 中配置的页面名)
 *===========================================================================*/
#define PAGE_NAME_HOME      "home"
#define PAGE_NAME_CAL       "cal"
#define PAGE_NAME_TEST      "test"
#define PAGE_NAME_DUAL      "dual"
#define PAGE_NAME_SINGLE    "single"
#define PAGE_NAME_RUN       "run"
#define PAGE_NAME_RESULT    "result"

/*=============================================================================
 * 控件名称 (对应 DGUS 中控件的 ID)
 *===========================================================================*/
#define CTRL_TSTA           "tsta"
#define CTRL_TITLE          "ttitle"
#define CTRL_T0             "t0"
#define CTRL_T1             "t1"
#define CTRL_T2             "t2"
#define CTRL_T3             "t3"
#define CTRL_T4             "t4"
#define CTRL_T5             "t5"

/*=============================================================================
 * 状态文本常量
 *===========================================================================*/
#define STATUS_READY        "READY"
#define STATUS_CAL_READY    "CAL_READY"
#define STATUS_TEST_READY   "TEST_READY"
#define STATUS_DUAL_READY   "DUAL_READY"
#define STATUS_SINGLE_READY "SINGLE_READY"
#define STATUS_RUNNING      "RUNNING"

/*=============================================================================
 * 令牌定义 (屏幕通过 prints 发送的字符串)
 *===========================================================================*/
#define TOKEN_CAL           "CAL"
#define TOKEN_TEST          "TEST"
#define TOKEN_DUAL          "DUAL"
#define TOKEN_SINGLE        "SINGLE"
#define TOKEN_HOME          "HOME"
#define TOKEN_OPEN          "OPEN"
#define TOKEN_BACK          "BACK"
#define TOKEN_START         "START"
#define TOKEN_FULL          "FULL"
#define TOKEN_WIREMAP       "WIREMAP"
#define TOKEN_SHIELD        "SHIELD"
#define TOKEN_RES           "RES"
#define TOKEN_LOSS          "LOSS"
#define TOKEN_LEN           "LEN"
#define TOKEN_SHORT         "SHORT"
#define TOKEN_LOC           "LOC"

/*=============================================================================
 * 事件 ID
 *===========================================================================*/
typedef enum {
    UI_EVENT_NONE = 0,

    /* HMI 导航事件 */
    UI_EVENT_OPEN_CAL,          // CAL:OPEN
    UI_EVENT_OPEN_TEST,         // TEST:OPEN
    UI_EVENT_OPEN_DUAL,         // TEST:DUAL
    UI_EVENT_OPEN_SINGLE,       // TEST:SINGLE
    UI_EVENT_BACK_HOME,         // HOME:BACK

    /* 校准事件 */
    UI_EVENT_CAL_START,         // CAL:START:<长度>:<类型>

    /* 双端测试事件 */
    UI_EVENT_DUAL_FULL,         // DUAL:FULL
    UI_EVENT_DUAL_WIREMAP,      // DUAL:WIREMAP
    UI_EVENT_DUAL_SHIELD,       // DUAL:SHIELD
    UI_EVENT_DUAL_RESISTANCE,   // DUAL:RES
    UI_EVENT_DUAL_LOSS,         // DUAL:LOSS

    /* 单端测试事件 */
    UI_EVENT_SINGLE_FULL,       // SINGLE:FULL
    UI_EVENT_SINGLE_LENGTH,     // SINGLE:LEN
    UI_EVENT_SINGLE_SHORT_DETECT,   // SINGLE:SHORT
    UI_EVENT_SINGLE_SHORT_LOCATE,   // SINGLE:LOC

} UI_EventId;

/*=============================================================================
 * 校准类型
 *===========================================================================*/
#define CAL_TYPE_UTP     "UTP"
#define CAL_TYPE_SFTP    "SFTP"

/*=============================================================================
 * 解析辅助
 *===========================================================================*/
#define UI_TOKEN_MAX_PARTS  4
#define UI_TOKEN_PART_LEN   16
#define UI_CMD_BUF_LEN      64

#endif
