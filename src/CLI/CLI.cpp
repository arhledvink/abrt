#include <iostream>
#include <getopt.h>
#include "ABRTException.h"
#include "ABRTSocket.h"
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "DBusCommon.h"

enum
{
    HELP,
    GET_LIST,
    GET_LIST_FULL,
    REPORT,
    REPORT_ALWAYS,
    DELETE
};

static DBusConnection* s_dbus_conn;

static void print_crash_infos(const vector_crash_infos_t& pCrashInfos, int pMode)
{
    unsigned int ii;
    for (ii = 0; ii < pCrashInfos.size(); ii++)
    {
        if (pCrashInfos[ii].find(CD_REPORTED)->second[CD_CONTENT] != "1" || pMode == GET_LIST_FULL)
        {
            std::cout << ii << ".\n";
            std::cout << "\tUID       : " << pCrashInfos[ii].find(CD_UID)->second[CD_CONTENT] << std::endl;
            std::cout << "\tUUID      : " << pCrashInfos[ii].find(CD_UUID)->second[CD_CONTENT] << std::endl;
            std::cout << "\tPackage   : " << pCrashInfos[ii].find(CD_PACKAGE)->second[CD_CONTENT] << std::endl;
            std::cout << "\tExecutable: " << pCrashInfos[ii].find(CD_EXECUTABLE)->second[CD_CONTENT] << std::endl;
            std::cout << "\tCrash time: " << pCrashInfos[ii].find(CD_TIME)->second[CD_CONTENT] << std::endl;
            std::cout << "\tCrash Rate: " << pCrashInfos[ii].find(CD_COUNT)->second[CD_CONTENT] << std::endl;
        }
    }
}

static void print_crash_report(const map_crash_report_t& pCrashReport)
{
    map_crash_report_t::const_iterator it = pCrashReport.begin();
    for (; it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] != CD_SYS)
        {
            std::cout << std::endl << it->first << std::endl;
            std::cout << "-----" << std::endl;
            std::cout << it->second[CD_CONTENT] << std::endl;
        }
    }
}

/*
 * DBus member calls
 */

/* helpers */
static DBusMessage* new_call_msg(const char* method)
{
    DBusMessage* msg = dbus_message_new_method_call(CC_DBUS_NAME, CC_DBUS_PATH, CC_DBUS_IFACE, method);
    if (!msg)
        die_out_of_memory();
    return msg;
}
static DBusMessage* send_get_reply_and_unref(DBusMessage* msg)
{
    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(s_dbus_conn, msg, /*timeout*/ -1, &err);
    if (reply == NULL)
    {
//analyse error
        error_msg_and_die("Error sending DBus message");
    }
    dbus_message_unref(msg);
    return reply;
}

static vector_crash_infos_t call_GetCrashInfos()
{
    DBusMessage* msg = new_call_msg("GetCrashInfos");

    DBusMessage *reply = send_get_reply_and_unref(msg);

    vector_crash_infos_t argout;
    DBusMessageIter in_iter;
    if (!dbus_message_iter_init(reply, &in_iter)) /* no values */
        error_msg_and_die("dbus call %s: return type mismatch", "GetCrashInfos");
    int r = load_val(&in_iter, argout);
    if (r != ABRT_DBUS_LAST_FIELD) /* more values present, or bad type */
        error_msg_and_die("dbus call %s: return type mismatch", "GetCrashInfos");
    dbus_message_unref(reply);
    return argout;
}

static map_crash_report_t call_CreateReport(const char* uuid)
{
    DBusMessage* msg = new_call_msg("GetJobResult");
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &uuid,
            DBUS_TYPE_INVALID);

    DBusMessage *reply = send_get_reply_and_unref(msg);

    map_crash_report_t argout;
    DBusMessageIter in_iter;
    if (!dbus_message_iter_init(reply, &in_iter)) /* no values */
        error_msg_and_die("dbus call %s: return type mismatch", "GetJobResult");
    int r = load_val(&in_iter, argout);
    if (r != ABRT_DBUS_LAST_FIELD) /* more values present, or bad type */
        error_msg_and_die("dbus call %s: return type mismatch", "GetJobResult");
    dbus_message_unref(reply);
    return argout;
}

static void call_Report(const map_crash_report_t& report)
{
    DBusMessage* msg = new_call_msg("Report");
    DBusMessageIter out_iter;
    dbus_message_iter_init_append(msg, &out_iter);
    store_val(&out_iter, report);

    DBusMessage *reply = send_get_reply_and_unref(msg);
    //it returns a single value of report_status_t type,
    //but we don't use it (yet?)

    dbus_message_unref(reply);
    return;
}

static void call_DeleteDebugDump(const char* uuid)
{
    DBusMessage* msg = new_call_msg("DeleteDebugDump");
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &uuid,
            DBUS_TYPE_INVALID);

    DBusMessage *reply = send_get_reply_and_unref(msg);
    //it returns a single boolean value,
    //but we don't use it (yet?)

    dbus_message_unref(reply);
    return;
}

static void handle_dbus_err(bool error_flag, DBusError *err)
{
    if (dbus_error_is_set(err))
    {
        error_msg("dbus error: %s", err->message);
        /* dbus_error_free(&err); */
        error_flag = true;
    }
    if (!error_flag)
        return;
    error_msg_and_die(
            "error requesting DBus name %s, possible reasons: "
            "abrt run by non-root; dbus config is incorrect",
            CC_DBUS_NAME);
}

static const struct option longopts[] =
{
    /* name, has_arg, flag, val */
    { "help"         , no_argument      , NULL, HELP          },
    { "version"      , no_argument      , NULL, HELP          },
    { "get-list"     , no_argument      , NULL, GET_LIST      },
    { "get-list-full", no_argument      , NULL, GET_LIST_FULL },
    { "report"       , required_argument, NULL, REPORT        },
    { "report-always", required_argument, NULL, REPORT_ALWAYS },
    { "delete"       , required_argument, NULL, DELETE        },
};

int main(int argc, char** argv)
{
    char* uuid = NULL;
    int op = -1;

    while (1)
    {
        int option_index;
        int c = getopt_long_only(argc, argv, "", longopts, &option_index);
        switch (c)
        {
            case REPORT:
            case REPORT_ALWAYS:
            case DELETE:
                uuid = optarg;
                /* fall through */
            case GET_LIST:
            case GET_LIST_FULL:
                if (op == -1)
                    break;
                /* fall through */
            case -1: /* end of options */
                if (op != -1)
                    break;
                error_msg("You must specify exactly one operation.");
                /* fall through */
            default:
            case HELP:
                char* progname = strrchr(argv[0], '/');
                if (progname)
                    progname++;
                else
                    progname = argv[0];
                /* note: message has embedded tabs */
                std::cout << "Usage: " << progname << " [OPTION]\n\n"
                        "	--get-list		print list of crashes which are not reported\n"
                        "	--get-list-full		print list of all crashes\n"
                        "	--report UUID		create and send a report\n"
                        "	--report-always UUID	create and send a report without asking\n"
                        "	--delete UUID		delete crash\n";
                return 1;
        }
        if (c == -1)
            break;
        op = c;
    }

#ifdef ENABLE_DBUS
    DBusError err;
    dbus_error_init(&err);
    s_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    handle_dbus_err(s_dbus_conn == NULL, &err);
#elif ENABLE_SOCKET
    CABRTSocket ABRTDaemon;
    ABRTDaemon.Connect(VAR_RUN"/abrt.socket");
#endif
    switch (op)
    {
        case GET_LIST:
        case GET_LIST_FULL:
        {
            vector_crash_infos_t ci = call_GetCrashInfos();
            print_crash_infos(ci, op);
            break;
        }
        case REPORT:
        {
            map_crash_report_t cr = call_CreateReport(uuid);
            print_crash_report(cr);
            std::cout << "\nDo you want to send the report? [y/n]: ";
            std::flush(std::cout);
            std::string answer = "n";
            std::cin >> answer;
            if (answer == "Y" || answer == "y")
            {
                call_Report(cr);
            }
            break;
        }
        case REPORT_ALWAYS:
        {
            map_crash_report_t cr = call_CreateReport(uuid);
            call_Report(cr);
            break;
        }
        case DELETE:
        {
            call_DeleteDebugDump(uuid);
            break;
        }
    }
#if ENABLE_SOCKET
    ABRTDaemon.DisConnect();
#endif

    return 0;
}
