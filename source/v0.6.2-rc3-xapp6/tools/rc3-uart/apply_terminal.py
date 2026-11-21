"""Connect the external Terminal window and the common command registry."""

from pathlib import Path
import re
import sys

source = Path(sys.argv[1])
files = {}


def edit(name, before, after):
    if name not in files:
        files[name] = (source / name).read_text(encoding="utf-8")
    text = files[name]
    if text.count(before) != 1:
        raise RuntimeError(f"{name}: expected one Terminal anchor {before!r}")
    files[name] = text.replace(before, after)


uart = "uart_service.h"
edit(uart, "void XdosPrintBenchmarkReport(CSerialDevice &serial);", """struct XdosCommandReply {
    void *context;
    void (*send)(void *,const char *);
    void Write(const char *message) const { send(context,message); }
};
void XdosPrintBenchmarkReport(XdosCommandReply reply);""")
edit("benchmark_report.inc", "void XdosPrintBenchmarkReport(CSerialDevice &serial)",
     "void XdosPrintBenchmarkReport(XdosCommandReply reply)")
files["benchmark_report.inc"] = files["benchmark_report.inc"].replace(
    "serial.Write(line, line.GetLength());", "reply.Write(line);")

commands = (
    ("Help", "HELP", False, True), ("Ping", "PING", False, True),
    ("Power", "POWER", False, True), ("Lvgl", "LVGL", False, True),
    ("BenchResult", "BENCH-RESULT", False, True), ("Open", "OPEN", True, False),
    ("Close", "CLOSE", False, False), ("Fetch", "FETCH", True, False),
    ("FetchStatus", "FETCH-STATUS", False, False), ("Reset", "RESET", False, False),
    ("Cancel", "CANCEL", False, False), ("Boot", "BOOT", True, False),
    ("BootClear", "BOOT-CLEAR", False, False), ("Put", "PUT", True, False),
    ("Data", "DATA", True, False), ("End", "END", False, False),
)
if '"HDMI-MODE ",10' in files[uart]:
    commands += (("HdmiMode", "HDMI-MODE", True, False),)

registry = """    enum class Command { Unknown, """ + ", ".join(row[0] for row in commands) + """ };
    struct CommandEntry { Command command; const char *name; bool arguments; bool terminal; };
    static constexpr CommandEntry commandTable_[] = {
""" + "".join(f'        {{Command::{symbol}, "{name}", {str(args).lower()}, {str(local).lower()}}},\n'
               for symbol, name, args, local in commands) + """    };

    static const CommandEntry *FindCommand(const char *line)
    {
        for (const auto &entry : commandTable_) {
            const auto length = strlen(entry.name);
            if (!strncmp(line,entry.name,length) &&
                line[length] == (entry.arguments ? ' ' : '\\0')) return &entry;
        }
        return nullptr;
    }

    void PrintHelp(XdosCommandReply reply, bool terminal)
    {
        for (const auto &entry : commandTable_) {
            if (terminal && !entry.terminal) continue;
            reply.Write(entry.name);
            reply.Write(entry.arguments ? " <args>\\r\\n" : "\\r\\n");
        }
        reply.Write("@XDOS OK\\r\\n");
    }

"""
text = files[uart]
start = text.index("    void Dispatch(")
dispatch = text[start:]
for symbol, name, args, _ in commands:
    pattern = (rf'!strncmp\(cmd,\s*"{name} ",\s*{len(name)+1}\)'
               if args else rf'!strcmp\(cmd,\s*"{name}"\)')
    dispatch = re.sub(pattern, f"command==Command::{symbol}", dispatch)

blocks = []
for name in ("Ping", "Power", "BenchResult", "Lvgl"):
    block = re.search(rf"^        if \(command==Command::{name}\) \{{.*?^        \}}\n",
                      dispatch, flags=re.S | re.M)
    if not block:
        raise RuntimeError(f"Missing shared query {name}")
    blocks.append(block[0].replace("Reply(serial, ", "reply.Write(")
                  .replace("ReportPower(serial)", "ReportPower(reply)")
                  .replace("XdosPrintBenchmarkReport(serial)", "XdosPrintBenchmarkReport(reply)")
                  .replace("            return;", "            return true;"))
dispatch = re.sub(r'        if \(command==Command::(?:Ping|Power|BenchResult|Lvgl)\) \{.*?^        \}\n',
                  '', dispatch, flags=re.S | re.M)
dispatch = dispatch.replace("        char *cmd = line_ + 6;\n", """        char *cmd = line_ + 6;
        const auto *entry = FindCommand(cmd);
        const auto command = entry ? entry->command : Command::Unknown;
        const XdosCommandReply reply{&serial, [](void *port,const char *message) {
            Reply(*static_cast<CSerialDevice *>(port),message);
        }};
        if (RunQuery(command,reply,false)) return;
""")
query = """    bool RunQuery(Command command, XdosCommandReply reply, bool terminal)
    {
        if (command==Command::Help) {
            PrintHelp(reply,terminal);
            return true;
        }
""" + "".join(blocks) + """        return false;
    }

"""
files[uart] = text[:start] + registry + query + dispatch
edit(uart, "    static void ReportPower(CSerialDevice &serial)",
     "    static void ReportPower(XdosCommandReply reply)")
edit(uart, 'Reply(serial, status);', 'reply.Write(status);')
edit(uart, r'''            Reply(serial, "@XDOS ERROR\r\n");
            return;
        }
        char status[] = "@XDOS POWER''', r'''            reply.Write("@XDOS ERROR\r\n");
            return;
        }
        char status[] = "@XDOS POWER''')
edit(uart, "class XdosUartService {\npublic:\n", """class XdosUartService {
public:
    // Returns false only for unregistered legacy desktop commands.
    bool RunTerminalCommand(const char *line, XdosCommandReply reply)
    {
        const auto *entry = FindCommand(line);
        if (!entry) return false;
        if (entry->terminal) RunQuery(entry->command,reply,true);
        else reply.Write("This command is available over the debug UART only.\\r\\n");
        return true;
    }

""")
edit(uart, "Widgets, Benchmark, Close", "Widgets, Benchmark, Close, Terminal")
edit(uart, '{"WIDGETS",Shortcut::Widgets}', '{"TERMINAL",Shortcut::Terminal}, {"WIDGETS",Shortcut::Widgets}')

edit("desktop.h", "FilesApp, BrowserApp", "FilesApp, BrowserApp, TerminalApp")
edit("desktop.h", "char terminal_[2048]", "char terminal_[8192]")
edit("desktop.cpp", "    if (i == Paint || i == Files || i == Browser) {",
     """    if (i == Terminal) {
        workspacePending_ = TerminalApp;
        return;
    }
    if (i == Paint || i == Files || i == Browser) {""")
edit("app_shortcut.cpp", "    if (!app || app > sizeof entries / sizeof entries[0])", """    if (app==4) {
        const auto staged=XappLoaderV1::ExecuteFile("SD:/XDOS/INBOX/TMTERM.XAPP",api,true);
        if (staged==XappLoaderV1::OK) return "XAPP: Terminal started";
        static const char module[]="SD:/XDOS/APPS/TERMINAL.XAPP";
        if (VerifyManifest("SD:/XDOS/APPS/TERMINAL.APP",module) &&
            XappLoaderV1::ExecuteFile(module,api,true)==XappLoaderV1::OK)
            return "XAPP: Terminal started";
        return "XAPP: Terminal module missing or incompatible";
    }
    if (!app || app > sizeof entries / sizeof entries[0])""")
edit("kernel.cpp", "    static XdosUartService uart;", "    static XdosUartService &uart = TerminalUart();")
edit("kernel.cpp", '#include "benchmark_report.inc"', """#include "benchmark_report.inc"

static XdosUartService &TerminalUart()
{
    static XdosUartService uart;
    return uart;
}""")
edit("kernel.cpp", "            case Shortcut::Browser: workspace=Desktop::BrowserApp; break;",
     """            case Shortcut::Browser: workspace=Desktop::BrowserApp; break;
            case Shortcut::Terminal: workspace=Desktop::TerminalApp; break;""")
edit("kernel.cpp", "                            case XDOS_REQUEST_LIST: return k->desktop_.RequestList() ? 0 : -1;",
     r"""                            case XDOS_REQUEST_TERMINAL: {
                                if (!text || !*text || strlen(text)>=160) return -1;
                                const XdosCommandReply reply{&k->desktop_,[](void *context,const char *message) {
                                    static_cast<Desktop *>(context)->Print(message);
                                }};
                                char command[160];
                                const char *p=text;
                                while (*p==' ' || *p=='\t') ++p;
                                unsigned used=0;
                                for (; *p; ++p) {
                                    const char ch=*p;
                                    if (ch=='\r' || ch=='\n') return -1;
                                    command[used++]=ch>='a' && ch<='z' ? ch-'a'+'A' : ch;
                                }
                                while (used && command[used-1]==' ') --used;
                                command[used]=0;
                                if (!TerminalUart().RunTerminalCommand(command,reply)) {
                                    k->desktop_.RunLegacyTerminal(text);
                                }
                                return 0;
                            }
                            case XDOS_REQUEST_LIST: return k->desktop_.RequestList() ? 0 : -1;""")
edit("desktop.h", "    const char *TerminalText() const", "    void RunLegacyTerminal(const char *text);\n    const char *TerminalText() const")
edit("desktop.cpp", "void Desktop::Key(char ch)", r"""void Desktop::RunLegacyTerminal(const char *text)
{
    cmdLen_=0;
    cmd_[0]=0;
    for (; *text; ++text) Key(*text);
    Key('\n');
}

void Desktop::Key(char ch)""")
edit("xdos_lvgl.cpp", "panel > XDOS_PANEL_HTTP", "(panel > XDOS_PANEL_HTTP && panel != XDOS_PANEL_TERMINAL)")
edit("xdos_lvgl.cpp", "    const bool files = workspacePanel_ == XDOS_PANEL_FILES;", """    if (workspacePanel_ == XDOS_PANEL_TERMINAL) {
        const auto revision=desktop_->TerminalRevision();
        if (revision!=workspaceRevision_) {
            workspaceRevision_=revision;
            lv_textarea_set_text(workspaceOutput_,desktop_->TerminalText());
            lv_textarea_set_cursor_pos(workspaceOutput_,LV_TEXTAREA_CURSOR_LAST);
        }
        return;
    }
    const bool files = workspacePanel_ == XDOS_PANEL_FILES;""")
edit("xdos_lvgl.cpp", "    const auto result = workspaceAction_(workspaceRequest_,workspaceContext_,index,\n"
     "                                        lv_textarea_get_text(workspaceInput_));",
     """    const auto result = workspaceAction_(workspaceRequest_,workspaceContext_,index,
                                        lv_textarea_get_text(workspaceInput_));
    if (!result && workspacePanel_==XDOS_PANEL_TERMINAL) lv_textarea_set_text(workspaceInput_,"");""")

for name, text in files.items():
    (source / name).write_text(text, encoding="utf-8")
