// @generated SignedSource<<70aa43dfd3cb64c6364e0dc06efa5e68>>

#include <stdexcept>
#include <profilo/entries/EntryType.h>

namespace facebook {
namespace profilo {
namespace entries {

const char* to_string(EntryType type) {
  switch(type) {
    case UNKNOWN_TYPE: return "UNKNOWN_TYPE";
    case UI_INPUT_START: return "UI_INPUT_START";
    case UI_INPUT_END: return "UI_INPUT_END";
    case UI_UPDATE_START: return "UI_UPDATE_START";
    case UI_UPDATE_END: return "UI_UPDATE_END";
    case NET_ADDED: return "NET_ADDED";
    case NET_ERROR: return "NET_ERROR";
    case NET_END: return "NET_END";
    case NET_RESPONSE: return "NET_RESPONSE";
    case NET_RETRY: return "NET_RETRY";
    case NET_START: return "NET_START";
    case NET_COUNTER: return "NET_COUNTER";
    case CALL_START: return "CALL_START";
    case CALL_END: return "CALL_END";
    case ASYNC_CALL: return "ASYNC_CALL";
    case SERV_CONN: return "SERV_CONN";
    case SERV_DISCONN: return "SERV_DISCONN";
    case SERV_END: return "SERV_END";
    case ADAPTER_NOTIFY: return "ADAPTER_NOTIFY";
    case ASYNC_TASK_PRE: return "ASYNC_TASK_PRE";
    case ASYNC_TASK_BACKGROUND: return "ASYNC_TASK_BACKGROUND";
    case ASYNC_TASK_POST: return "ASYNC_TASK_POST";
    case ASYNC_TASK_END: return "ASYNC_TASK_END";
    case NETWORK_OP_START: return "NETWORK_OP_START";
    case NETWORK_OP_END: return "NETWORK_OP_END";
    case MARK_FLAG: return "MARK_FLAG";
    case MARK_START: return "MARK_START";
    case MARK_STOP: return "MARK_STOP";
    case MARK_FAIL: return "MARK_FAIL";
    case MARK_CANCEL: return "MARK_CANCEL";
    case MARK_PUSH: return "MARK_PUSH";
    case MARK_POP: return "MARK_POP";
    case LIFECYCLE_APPLICATION_START: return "LIFECYCLE_APPLICATION_START";
    case LIFECYCLE_APPLICATION_END: return "LIFECYCLE_APPLICATION_END";
    case LIFECYCLE_ACTIVITY_START: return "LIFECYCLE_ACTIVITY_START";
    case LIFECYCLE_ACTIVITY_END: return "LIFECYCLE_ACTIVITY_END";
    case LIFECYCLE_SERVICE_START: return "LIFECYCLE_SERVICE_START";
    case LIFECYCLE_SERVICE_END: return "LIFECYCLE_SERVICE_END";
    case LIFECYCLE_BROADCAST_RECEIVER_START: return "LIFECYCLE_BROADCAST_RECEIVER_START";
    case LIFECYCLE_BROADCAST_RECEIVER_END: return "LIFECYCLE_BROADCAST_RECEIVER_END";
    case LIFECYCLE_CONTENT_PROVIDER_START: return "LIFECYCLE_CONTENT_PROVIDER_START";
    case LIFECYCLE_CONTENT_PROVIDER_END: return "LIFECYCLE_CONTENT_PROVIDER_END";
    case LIFECYCLE_FRAGMENT_START: return "LIFECYCLE_FRAGMENT_START";
    case LIFECYCLE_FRAGMENT_END: return "LIFECYCLE_FRAGMENT_END";
    case LIFECYCLE_VIEW_START: return "LIFECYCLE_VIEW_START";
    case LIFECYCLE_VIEW_END: return "LIFECYCLE_VIEW_END";
    case TRACE_ABORT: return "TRACE_ABORT";
    case TRACE_END: return "TRACE_END";
    case TRACE_START: return "TRACE_START";
    case TRACE_BACKWARDS: return "TRACE_BACKWARDS";
    case TRACE_TIMEOUT: return "TRACE_TIMEOUT";
    case COUNTER: return "COUNTER";
    case STACK_FRAME: return "STACK_FRAME";
    case TEST_CLOCK_SYNC_START: return "TEST_CLOCK_SYNC_START";
    case TEST_CLOCK_SYNC_END: return "TEST_CLOCK_SYNC_END";
    case QPL_START: return "QPL_START";
    case QPL_END: return "QPL_END";
    case QPL_CANCEL: return "QPL_CANCEL";
    case QPL_NOTE: return "QPL_NOTE";
    case QPL_POINT: return "QPL_POINT";
    case TRACE_ANNOTATION: return "TRACE_ANNOTATION";
    case WAIT_START: return "WAIT_START";
    case WAIT_END: return "WAIT_END";
    case WAIT_SIGNAL: return "WAIT_SIGNAL";
    case STRING_KEY: return "STRING_KEY";
    case STRING_VALUE: return "STRING_VALUE";
    case QPL_TAG: return "QPL_TAG";
    case QPL_ANNOTATION: return "QPL_ANNOTATION";
    case TRACE_THREAD_NAME: return "TRACE_THREAD_NAME";
    case TRACE_PRE_END: return "TRACE_PRE_END";
    case TRACE_THREAD_PRI: return "TRACE_THREAD_PRI";
    case MAJOR_FAULT: return "MAJOR_FAULT";
    case YARN_LOST_RECORDS: return "YARN_LOST_RECORDS";
    case CLASS_LOAD: return "CLASS_LOAD";
    case NATIVE_STACK_FRAME: return "NATIVE_STACK_FRAME";
    case NATIVE_LIB_MAPPING: return "NATIVE_LIB_MAPPING";
    case JAVASCRIPT_STACK_FRAME: return "JAVASCRIPT_STACK_FRAME";
    case MESSAGE_START: return "MESSAGE_START";
    case MESSAGE_END: return "MESSAGE_END";
    case CLASS_VALUE: return "CLASS_VALUE";
    case HTTP2_REQUEST_INITIATED: return "HTTP2_REQUEST_INITIATED";
    case HTTP2_FRAME_HEADER: return "HTTP2_FRAME_HEADER";
    case HTTP2_WINDOW_UPDATE: return "HTTP2_WINDOW_UPDATE";
    case HTTP2_PRIORITY: return "HTTP2_PRIORITY";
    case PROCESS_LIST: return "PROCESS_LIST";
    case IO_START: return "IO_START";
    case IO_END: return "IO_END";
    case CPU_COUNTER: return "CPU_COUNTER";
    default: throw std::invalid_argument("Unknown entry type");
  }
}

} // namespace entries
} // namespace profilo
} // namespace facebook
