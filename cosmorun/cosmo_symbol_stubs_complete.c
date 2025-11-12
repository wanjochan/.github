/* Auto-generated complete symbol stubs for CosmoRun */
/* Total symbols: 444 */
/* Generated to resolve all remaining undefined references */

#include <stddef.h>
#include <stdint.h>

/* Suppress compiler warnings */
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"



/* ACPI Variables (6 symbols) */
__attribute__((weak)) void* _AcpiBootFlags = NULL;
__attribute__((weak)) void* _AcpiIoApics = NULL;
__attribute__((weak)) void* _AcpiMadtFlags = NULL;
__attribute__((weak)) void* _AcpiNumIoApics = NULL;
__attribute__((weak)) void* _AcpiXsdtEntries = NULL;
__attribute__((weak)) void* _AcpiXsdtNumEntries = NULL;

/* C++ ABI (5 symbols) */
__attribute__((weak)) void* __cxa_get_globals = NULL;
__attribute__((weak)) void* __cxa_get_globals_fast = NULL;
__attribute__((weak)) void* __cxa_new_handler = NULL;
__attribute__((weak)) void* __cxa_terminate_handler = NULL;
__attribute__((weak)) void* __cxa_unexpected_handler = NULL;

/* Constants (12 symbols) */
__attribute__((weak)) const long AT_BASE_PLATFORM = 0;
__attribute__((weak)) const long AT_DCACHEBSIZE = 0;
__attribute__((weak)) const long AT_ICACHEBSIZE = 0;
__attribute__((weak)) const long AT_MINSIGSTKSZ = 0;
__attribute__((weak)) const long AT_PAGESIZESLEN = 0;
__attribute__((weak)) const long AT_UCACHEBSIZE = 0;
__attribute__((weak)) const long F_BARRIERFSYNC = 0;
__attribute__((weak)) const long F_DUPFD_CLOEXEC = 0;
__attribute__((weak)) const long F_GETNOSIGPIPE = 0;
__attribute__((weak)) const long F_SETNOSIGPIPE = 0;
__attribute__((weak)) const long kNtSystemDirectory = 0;
__attribute__((weak)) const long kNtWindowsDirectory = 0;

/* Diagnostic Functions (30 symbols) */
__attribute__((weak)) const char* _DescribeDnotifyFlags(void* x) { return "_DescribeDnotifyFlags"; }
__attribute__((weak)) const char* _DescribeFcntlCmd(void* x) { return "_DescribeFcntlCmd"; }
__attribute__((weak)) const char* _DescribeFlockType(void* x) { return "_DescribeFlockType"; }
__attribute__((weak)) const char* _DescribeGidList(void* x) { return "_DescribeGidList"; }
__attribute__((weak)) const char* _DescribeInOutInt64(void* x) { return "_DescribeInOutInt64"; }
__attribute__((weak)) const char* _DescribeItimer(void* x) { return "_DescribeItimer"; }
__attribute__((weak)) const char* _DescribeItimerval(void* x) { return "_DescribeItimerval"; }
__attribute__((weak)) const char* _DescribeMapping(void* x) { return "_DescribeMapping"; }
__attribute__((weak)) const char* _DescribeMsyncFlags(void* x) { return "_DescribeMsyncFlags"; }
__attribute__((weak)) const char* _DescribeNtConsoleInFlags(void* x) { return "_DescribeNtConsoleInFlags"; }
__attribute__((weak)) const char* _DescribeNtConsoleOutFlags(void* x) { return "_DescribeNtConsoleOutFlags"; }
__attribute__((weak)) const char* _DescribeOpenFlags(void* x) { return "_DescribeOpenFlags"; }
__attribute__((weak)) const char* _DescribeOpenMode(void* x) { return "_DescribeOpenMode"; }
__attribute__((weak)) const char* _DescribePollFds(void* x) { return "_DescribePollFds"; }
__attribute__((weak)) const char* _DescribePtrace(void* x) { return "_DescribePtrace"; }
__attribute__((weak)) const char* _DescribeRlimit(void* x) { return "_DescribeRlimit"; }
__attribute__((weak)) const char* _DescribeRlimitName(void* x) { return "_DescribeRlimitName"; }
__attribute__((weak)) const char* _DescribeSeccompOperation(void* x) { return "_DescribeSeccompOperation"; }
__attribute__((weak)) const char* _DescribeSiCode(void* x) { return "_DescribeSiCode"; }
__attribute__((weak)) const char* _DescribeSiginfo(void* x) { return "_DescribeSiginfo"; }
__attribute__((weak)) const char* _DescribeSocketFamily(void* x) { return "_DescribeSocketFamily"; }
__attribute__((weak)) const char* _DescribeSocketProtocol(void* x) { return "_DescribeSocketProtocol"; }
__attribute__((weak)) const char* _DescribeSocketType(void* x) { return "_DescribeSocketType"; }
__attribute__((weak)) const char* _DescribeStatfs(void* x) { return "_DescribeStatfs"; }
__attribute__((weak)) const char* _DescribeStringList(void* x) { return "_DescribeStringList"; }
__attribute__((weak)) const char* _DescribeTermios(void* x) { return "_DescribeTermios"; }
__attribute__((weak)) const char* _DescribeTimeval(void* x) { return "_DescribeTimeval"; }
__attribute__((weak)) const char* _DescribeWhence(void* x) { return "_DescribeWhence"; }
__attribute__((weak)) const char* _DescribeWhichPrio(void* x) { return "_DescribeWhichPrio"; }
__attribute__((weak)) const char* _DescribeWinsize(void* x) { return "_DescribeWinsize"; }

/* ELF Functions (2 symbols) */
__attribute__((weak)) long GetElfSectionAddress(void) { return 0; }
__attribute__((weak)) long GetElfSymbolTable(void) { return 0; }

/* Other Functions (325 symbols) */
__attribute__((weak)) int AdjustTokenPrivileges(void) { return 0; }
__attribute__((weak)) int ClearCommBreak(void) { return 0; }
__attribute__((weak)) int CreateDirectory(void) { return 0; }
__attribute__((weak)) int CreateHardLink(void) { return 0; }
__attribute__((weak)) int CreateNamedPipe(void) { return 0; }
__attribute__((weak)) int CreateSymbolicLink(void) { return 0; }
__attribute__((weak)) int CreateWaitableTimer(void) { return 0; }
__attribute__((weak)) int DuplicateToken(void) { return 0; }
__attribute__((weak)) int FlushConsoleInputBuffer(void) { return 0; }
__attribute__((weak)) int FlushFileBuffers(void) { return 0; }
__attribute__((weak)) int FlushViewOfFile(void) { return 0; }
__attribute__((weak)) long GetAdaptersAddresses(void) { return 0; }
__attribute__((weak)) long GetComputerNameEx(void) { return 0; }
__attribute__((weak)) long GetCurrentDirectory(void) { return 0; }
__attribute__((weak)) long GetCurrentProcessId(void) { return 0; }
__attribute__((weak)) long GetExitCodeProcess(void) { return 0; }
__attribute__((weak)) long GetFileAttributes(void) { return 0; }
__attribute__((weak)) long GetFileAttributesEx(void) { return 0; }
__attribute__((weak)) long GetFileSecurity(void) { return 0; }
__attribute__((weak)) long GetFinalPathNameByHandle(void) { return 0; }
__attribute__((weak)) long GetInterpreterExecutableName(void) { return 0; }
__attribute__((weak)) long GetLogicalDrives(void) { return 0; }
__attribute__((weak)) long GetModuleFileName(void) { return 0; }
__attribute__((weak)) long GetNtOpenFlags(void) { return 0; }
__attribute__((weak)) long GetNumaProcessorNodeEx(void) { return 0; }
__attribute__((weak)) long GetNumberOfConsoleInputEvents(void) { return 0; }
__attribute__((weak)) long GetPriorityClass(void) { return 0; }
__attribute__((weak)) long GetProcAddress(void) { return 0; }
__attribute__((weak)) long GetProcessHeap(void) { return 0; }
__attribute__((weak)) long GetProcessIoCounters(void) { return 0; }
__attribute__((weak)) long GetProcessMemoryInfo(void) { return 0; }
__attribute__((weak)) long GetProcessTimes(void) { return 0; }
__attribute__((weak)) long GetStartupInfo(void) { return 0; }
__attribute__((weak)) long GetSymbolByAddr(void) { return 0; }
__attribute__((weak)) long GetSymbolTable(void) { return 0; }
__attribute__((weak)) long GetSystemTimePreciseAsFileTime(void) { return 0; }
__attribute__((weak)) long GetSystemTimes(void) { return 0; }
__attribute__((weak)) long GetTickCount64(void) { return 0; }
__attribute__((weak)) long GetTimeZoneInformation(void) { return 0; }
__attribute__((weak)) long GetZipCdirOffset(void) { return 0; }
__attribute__((weak)) long GetZipCdirRecords(void) { return 0; }
__attribute__((weak)) long GetZipCfileMode(void) { return 0; }
__attribute__((weak)) long GetZipCfileOffset(void) { return 0; }
__attribute__((weak)) long GetZipLfileCompressedSize(void) { return 0; }
__attribute__((weak)) long GetZipLfileUncompressedSize(void) { return 0; }
__attribute__((weak)) void GlobalMemoryStatusEx(void) {}
__attribute__((weak)) void IPV6_JOIN_GROUP(void) {}
__attribute__((weak)) void IPV6_LEAVE_GROUP(void) {}
__attribute__((weak)) void IPV6_MULTICAST_HOPS(void) {}
__attribute__((weak)) void IPV6_MULTICAST_IF(void) {}
__attribute__((weak)) void IPV6_MULTICAST_LOOP(void) {}
__attribute__((weak)) void IPV6_RECVRTHDR(void) {}
__attribute__((weak)) void IPV6_RECVTCLASS(void) {}
__attribute__((weak)) void IPV6_UNICAST_HOPS(void) {}
__attribute__((weak)) void IP_ADD_MEMBERSHIP(void) {}
__attribute__((weak)) void IP_DROP_MEMBERSHIP(void) {}
__attribute__((weak)) void IP_MULTICAST_IF(void) {}
__attribute__((weak)) void IP_MULTICAST_LOOP(void) {}
__attribute__((weak)) void IP_MULTICAST_TTL(void) {}
__attribute__((weak)) void ImpersonateSelf(void) {}
__attribute__((weak)) void InitiateShutdown(void) {}
__attribute__((weak)) void IsGenuineBlink(void) {}
__attribute__((weak)) void LookupPrivilegeValue(void) {}
__attribute__((weak)) void MapGenericMask(void) {}
__attribute__((weak)) long NtQueryInformationProcess(void) { return 0; }
__attribute__((weak)) long NtQueryVolumeInformationFile(void) { return 0; }
__attribute__((weak)) void OfferVirtualMemory(void) {}
__attribute__((weak)) void OpenProcessToken(void) {}
__attribute__((weak)) void PrefetchVirtualMemory(void) {}
__attribute__((weak)) void RLIMIT_MEMLOCK(void) {}
__attribute__((weak)) void RLIMIT_MSGQUEUE(void) {}
__attribute__((weak)) void RLIMIT_SIGPENDING(void) {}
__attribute__((weak)) void RUSAGE_CHILDREN(void) {}
__attribute__((weak)) void ReadConsoleInput(void) {}
__attribute__((weak)) void RegQueryInfoKey(void) {}
__attribute__((weak)) void RemoveDirectory(void) {}
__attribute__((weak)) int SetConsoleMode(void) { return 0; }
__attribute__((weak)) int SetCurrentDirectory(void) { return 0; }
__attribute__((weak)) int SetEnvironmentVariable(void) { return 0; }
__attribute__((weak)) int SetFileAttributes(void) { return 0; }
__attribute__((weak)) int SetFileInformationByHandle(void) { return 0; }
__attribute__((weak)) int SetSuspendState(void) { return 0; }
__attribute__((weak)) int SetWaitableTimer(void) { return 0; }
__attribute__((weak)) void TCP_DEFER_ACCEPT(void) {}
__attribute__((weak)) void TCP_NOTSENT_LOWAT(void) {}
__attribute__((weak)) void TCP_WINDOW_CLAMP(void) {}
__attribute__((weak)) void TerminateProcess(void) {}
__attribute__((weak)) int TimeSpecToWindowsTime(void) { return 0; }
__attribute__((weak)) void TransmitCommChar(void) {}
__attribute__((weak)) void VirtualProtectEx(void) {}
__attribute__((weak)) int WSACreateEvent(void) { return 0; }
__attribute__((weak)) int WSAGetLastError(void) { return 0; }
__attribute__((weak)) int WSAGetOverlappedResult(void) { return 0; }
__attribute__((weak)) void WSAWaitForMultipleEvents(void) {}
__attribute__((weak)) int WindowsDurationToTimeVal(void) { return 0; }
__attribute__((weak)) void WriteProcessMemory(void) {}
__attribute__((weak)) void _Z14__kmp_mwait_64ILb0ELb1EEviP11kmp_flag_64IXT_EXT0_EE(void) {}
__attribute__((weak)) void _Z15__kmp_resume_32ILb0ELb0EEviP11kmp_flag_32IXT_EXT0_EE(void) {}
__attribute__((weak)) void _Z15__kmp_resume_32ILb0ELb1EEviP11kmp_flag_32IXT_EXT0_EE(void) {}
__attribute__((weak)) void _Z15__kmp_resume_64ILb0ELb1EEviP11kmp_flag_64IXT_EXT0_EE(void) {}
__attribute__((weak)) void _Z16__kmp_suspend_64ILb0ELb1EEviP11kmp_flag_64IXT_EXT0_EE(void) {}
__attribute__((weak)) void _Z18__kmp_mwait_oncoreiP15kmp_flag_oncore(void) {}
__attribute__((weak)) void _Z21__kmp_atomic_mwait_64ILb0ELb1EEviP18kmp_atomic_flag_64IXT_EXT0_EE(void) {}
__attribute__((weak)) void _Z22__kmp_atomic_resume_64ILb0ELb1EEviP18kmp_atomic_flag_64IXT_EXT0_EE(void) {}
__attribute__((weak)) void _Z22__kmp_execute_tasks_32ILb0ELb0EEiP8kmp_infoiP11kmp_flag_32IXT_EXT0_EEiPii(void) {}
__attribute__((weak)) void _Z22__kmp_execute_tasks_64ILb0ELb1EEiP8kmp_infoiP11kmp_flag_64IXT_EXT0_EEiPii(void) {}
__attribute__((weak)) void _Z22__kmp_execute_tasks_64ILb1ELb0EEiP8kmp_infoiP11kmp_flag_64IXT_EXT0_EEiPii(void) {}
__attribute__((weak)) void _Z23__kmp_atomic_suspend_64ILb0ELb1EEviP18kmp_atomic_flag_64IXT_EXT0_EE(void) {}
__attribute__((weak)) void _Z29__kmp_atomic_execute_tasks_64ILb0ELb1EEiP8kmp_infoiP18kmp_atomic_flag_64IXT_EXT0_EEiPii(void) {}
__attribute__((weak)) void _ZN10__cxxabiv128__aligned_free_with_fallbackEPv(void) {}
__attribute__((weak)) void _ZN10__cxxabiv130__aligned_malloc_with_fallbackEm(void) {}
__attribute__((weak)) void _ZN17double_conversion23DoubleToStringConverter19EcmaScriptConverterEv(void) {}
__attribute__((weak)) void _ZNK17double_conversion23DoubleToStringConverter20ToShortestIeeeNumberEdPNS_13StringBuilderENS0_8DtoaModeE(void) {}
__attribute__((weak)) void _ZNK17double_conversion23StringToDoubleConverter14StringToDoubleEPKciPi(void) {}
__attribute__((weak)) void _ZNKSt3__114error_category10equivalentERKNS_10error_codeEi(void) {}
__attribute__((weak)) void _ZNKSt3__114error_category10equivalentEiRKNS_15error_conditionE(void) {}
__attribute__((weak)) void _ZNKSt3__114error_category23default_error_conditionEi(void) {}
__attribute__((weak)) void _ZNKSt3__14__fs10filesystem18directory_iterator13__dereferenceEv(void) {}
__attribute__((weak)) void _ZNKSt3__17collateIcE7do_hashEPKcS3_(void) {}
__attribute__((weak)) void _ZNKSt3__17collateIwE7do_hashEPKwS3_(void) {}
__attribute__((weak)) void _ZNSt3__110moneypunctIcLb0EE2idE(void) {}
__attribute__((weak)) void _ZNSt3__110moneypunctIcLb1EE2idE(void) {}
__attribute__((weak)) void _ZNSt3__110moneypunctIwLb0EE2idE(void) {}
__attribute__((weak)) void _ZNSt3__110moneypunctIwLb1EE2idE(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6__initEPKcm(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKc(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmmc(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9__grow_byEmmmmmm(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED1Ev(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE21__grow_by_and_replaceEmmmmmmPKw(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6insertEmmw(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE9__grow_byEmmmmmm(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE9push_backEw(void) {}
__attribute__((weak)) void _ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEED1Ev(void) {}
__attribute__((weak)) void _ZNSt3__113basic_istreamIcNS_11char_traitsIcEEED2Ev(void) {}
__attribute__((weak)) void _ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEE5flushEv(void) {}
__attribute__((weak)) void _ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEED2Ev(void) {}
__attribute__((weak)) void _ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEE5flushEv(void) {}
__attribute__((weak)) void _ZNSt3__114basic_iostreamIcNS_11char_traitsIcEEED2Ev(void) {}
__attribute__((weak)) void _ZNSt3__114error_categoryD2Ev(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE4swapERS3_(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE4syncEv(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE5imbueERKNS_6localeE(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE5uflowEv(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE6setbufEPcl(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE6xsgetnEPcl(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE6xsputnEPKcl(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE7seekoffExNS_8ios_base7seekdirEj(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE7seekposENS_4fposIjEEj(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE8overflowEi(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE9pbackfailEi(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE9showmanycEv(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE9underflowEv(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEEC2Ev(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEED2Ev(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE4syncEv(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE5uflowEv(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE6setbufEPwl(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE6xsgetnEPwl(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE6xsputnEPKwl(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE7seekoffExNS_8ios_base7seekdirEj(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE7seekposENS_4fposIjEEj(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE8overflowEj(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE9pbackfailEj(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE9showmanycEv(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE9underflowEv(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEEC2Ev(void) {}
__attribute__((weak)) void _ZNSt3__115basic_streambufIwNS_11char_traitsIwEEED2Ev(void) {}
__attribute__((weak)) void _ZNSt3__118condition_variable10notify_allEv(void) {}
__attribute__((weak)) void _ZNSt3__118condition_variable10notify_oneEv(void) {}
__attribute__((weak)) void _ZNSt3__118condition_variable4waitERNS_11unique_lockINS_5mutexEEE(void) {}
__attribute__((weak)) void _ZNSt3__118condition_variableD1Ev(void) {}
__attribute__((weak)) void _ZNSt3__14__fs10filesystem16filesystem_error13__create_whatEi(void) {}
__attribute__((weak)) void _ZNSt3__14__fs10filesystem16filesystem_errorD1Ev(void) {}
__attribute__((weak)) void _ZNSt3__14__fs10filesystem18directory_iterator11__incrementEPNS_10error_codeE(void) {}
__attribute__((weak)) void _ZNSt3__14__fs10filesystem18directory_iteratorC1ERKNS1_4pathEPNS_10error_codeENS1_17directory_optionsE(void) {}
__attribute__((weak)) void _ZNSt3__15mutexD1Ev(void) {}
__attribute__((weak)) void _ZNSt3__17collateIcE2idE(void) {}
__attribute__((weak)) void _ZNSt3__17collateIcED2Ev(void) {}
__attribute__((weak)) void _ZNSt3__17collateIwE2idE(void) {}
__attribute__((weak)) void _ZNSt3__17collateIwED2Ev(void) {}
__attribute__((weak)) void _ZNSt3__17num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__17num_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__17num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__17num_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__18messagesIcE2idE(void) {}
__attribute__((weak)) void _ZNSt3__18messagesIwE2idE(void) {}
__attribute__((weak)) void _ZNSt3__18time_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__18time_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__18time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__18time_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__19__num_putIcE21__widen_and_group_intEPcS2_S2_S2_RS2_S3_RKNS_6localeE(void) {}
__attribute__((weak)) void _ZNSt3__19__num_putIcE23__widen_and_group_floatEPcS2_S2_S2_RS2_S3_RKNS_6localeE(void) {}
__attribute__((weak)) void _ZNSt3__19__num_putIwE21__widen_and_group_intEPcS2_S2_PwRS3_S4_RKNS_6localeE(void) {}
__attribute__((weak)) void _ZNSt3__19__num_putIwE23__widen_and_group_floatEPcS2_S2_PwRS3_S4_RKNS_6localeE(void) {}
__attribute__((weak)) void _ZNSt3__19basic_iosIcNS_11char_traitsIcEEED2Ev(void) {}
__attribute__((weak)) void _ZNSt3__19basic_iosIwNS_11char_traitsIwEEED2Ev(void) {}
__attribute__((weak)) void _ZNSt3__19money_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__19money_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__19money_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__19money_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE(void) {}
__attribute__((weak)) void _ZNSt3__1plIcNS_11char_traitsIcEENS_9allocatorIcEEEENS_12basic_stringIT_T0_T1_EEPKS6_RKS9_(void) {}
__attribute__((weak)) void _ZTIN3ctl8ios_baseE(void) {}
__attribute__((weak)) void _ZTINSt3__112__do_messageE(void) {}
__attribute__((weak)) void _ZTINSt3__112system_errorE(void) {}
__attribute__((weak)) void _ZTINSt3__113basic_istreamIcNS_11char_traitsIcEEEE(void) {}
__attribute__((weak)) void _ZTINSt3__113basic_ostreamIcNS_11char_traitsIcEEEE(void) {}
__attribute__((weak)) void _ZTINSt3__114__shared_countE(void) {}
__attribute__((weak)) void _ZTINSt3__114basic_iostreamIcNS_11char_traitsIcEEEE(void) {}
__attribute__((weak)) void _ZTINSt3__114error_categoryE(void) {}
__attribute__((weak)) void _ZTINSt3__115basic_streambufIcNS_11char_traitsIcEEEE(void) {}
__attribute__((weak)) void _ZTINSt3__115basic_streambufIwNS_11char_traitsIwEEEE(void) {}
__attribute__((weak)) void _ZTINSt3__14__fs10filesystem16filesystem_errorE(void) {}
__attribute__((weak)) void _ZTINSt3__17collateIcEE(void) {}
__attribute__((weak)) void _ZTINSt3__17collateIwEE(void) {}
__attribute__((weak)) void _ZTVNSt3__110moneypunctIcLb0EEE(void) {}
__attribute__((weak)) void _ZTVNSt3__110moneypunctIcLb1EEE(void) {}
__attribute__((weak)) void _ZTVNSt3__110moneypunctIwLb0EEE(void) {}
__attribute__((weak)) void _ZTVNSt3__110moneypunctIwLb1EEE(void) {}
__attribute__((weak)) void _ZTVNSt3__112__do_messageE(void) {}
__attribute__((weak)) void _ZTVNSt3__113basic_istreamIcNS_11char_traitsIcEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__113basic_istreamIwNS_11char_traitsIwEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__113basic_ostreamIcNS_11char_traitsIcEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__113basic_ostreamIwNS_11char_traitsIwEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__114codecvt_bynameIDiDujEE(void) {}
__attribute__((weak)) void _ZTVNSt3__114codecvt_bynameIDicjEE(void) {}
__attribute__((weak)) void _ZTVNSt3__114codecvt_bynameIDsDujEE(void) {}
__attribute__((weak)) void _ZTVNSt3__114codecvt_bynameIDscjEE(void) {}
__attribute__((weak)) void _ZTVNSt3__114codecvt_bynameIccjEE(void) {}
__attribute__((weak)) void _ZTVNSt3__114codecvt_bynameIwcjEE(void) {}
__attribute__((weak)) void _ZTVNSt3__115messages_bynameIcEE(void) {}
__attribute__((weak)) void _ZTVNSt3__115messages_bynameIwEE(void) {}
__attribute__((weak)) void _ZTVNSt3__115time_get_bynameIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__115time_get_bynameIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__115time_put_bynameIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__115time_put_bynameIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__117moneypunct_bynameIcLb0EEE(void) {}
__attribute__((weak)) void _ZTVNSt3__117moneypunct_bynameIcLb1EEE(void) {}
__attribute__((weak)) void _ZTVNSt3__117moneypunct_bynameIwLb0EEE(void) {}
__attribute__((weak)) void _ZTVNSt3__117moneypunct_bynameIwLb1EEE(void) {}
__attribute__((weak)) void _ZTVNSt3__14__fs10filesystem16filesystem_errorE(void) {}
__attribute__((weak)) void _ZTVNSt3__17collateIcEE(void) {}
__attribute__((weak)) void _ZTVNSt3__17collateIwEE(void) {}
__attribute__((weak)) void _ZTVNSt3__17num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__17num_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__17num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__17num_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__18messagesIcEE(void) {}
__attribute__((weak)) void _ZTVNSt3__18messagesIwEE(void) {}
__attribute__((weak)) void _ZTVNSt3__18time_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__18time_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__18time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__18time_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__19money_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__19money_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__19money_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEEE(void) {}
__attribute__((weak)) void _ZTVNSt3__19money_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEEE(void) {}
__attribute__((weak)) void __create_pipe_name(void) {}
__attribute__((weak)) void __fixupnewsockfd(void) {}
__attribute__((weak)) void __get_avphys_pages(void) {}
__attribute__((weak)) void __get_rlimit(void) {}
__attribute__((weak)) void __is_stack_overflow(void) {}
__attribute__((weak)) void __libunwind_Registers_x86_64_jumpto(void) {}
__attribute__((weak)) void __localtime_lock(void) {}
__attribute__((weak)) void __localtime_unlock(void) {}
__attribute__((weak)) void __on_arithmetic_overflow(void) {}
__attribute__((weak)) void __sigsetjmp_tail(void) {}
__attribute__((weak)) void __sys_fcntl_cp(void) {}
__attribute__((weak)) void __sys_openat_nc(void) {}
__attribute__((weak)) void __uint64toarray_radix16(void) {}
__attribute__((weak)) void __unw_getcontext(void) {}
__attribute__((weak)) void __zipos_parseuri(void) {}
__attribute__((weak)) void __zipos_stat_impl(void) {}
__attribute__((weak)) void _init_metalfile(void) {}
__attribute__((weak)) int critbit0_clear(void) { return 0; }
__attribute__((weak)) int critbit0_emplace(void) { return 0; }
__attribute__((weak)) int ftrace_enabled = 0;
__attribute__((weak)) int ftrace_stackdigs = 0;
__attribute__((weak)) void func_a(void) {}
__attribute__((weak)) void getdomainname_linux(void) {}
__attribute__((weak)) void gethostname_bsd(void) {}
__attribute__((weak)) void gethostname_linux(void) {}
__attribute__((weak)) void gethostname_nt(void) {}
__attribute__((weak)) void getx86processormodel(void) {}
__attribute__((weak)) void isdirectory_nt(void) {}
__attribute__((weak)) void isregularfile_nt(void) {}
__attribute__((weak)) void iswalpha_l(void) {}
__attribute__((weak)) void iswblank_l(void) {}
__attribute__((weak)) void iswcntrl_l(void) {}
__attribute__((weak)) void iswdigit_l(void) {}
__attribute__((weak)) void iswlower_l(void) {}
__attribute__((weak)) void iswprint_l(void) {}
__attribute__((weak)) void iswpunct_l(void) {}
__attribute__((weak)) void iswspace_l(void) {}
__attribute__((weak)) void iswupper_l(void) {}
__attribute__((weak)) void iswxdigit_l(void) {}
__attribute__((weak)) void landlock_add_rule(void) {}
__attribute__((weak)) void landlock_create_ruleset(void) {}
__attribute__((weak)) void landlock_restrict_self(void) {}
__attribute__((weak)) void malloc_inspect_all(void) {}
__attribute__((weak)) void malloc_usable_size(void) {}
__attribute__((weak)) void statfs2statvfs(void) {}
__attribute__((weak)) void strcoll_l(void) {}
__attribute__((weak)) void strtod_l(void) {}
__attribute__((weak)) void strtof_l(void) {}
__attribute__((weak)) void strtold_l(void) {}
__attribute__((weak)) void strtoull(void) {}
__attribute__((weak)) void strtoull_l(void) {}
__attribute__((weak)) void strxfrm_l(void) {}
__attribute__((weak)) void tcgetwinsize_nt(void) {}
__attribute__((weak)) void tcsetwinsize_nt(void) {}
__attribute__((weak)) void timeval_frommicros(void) {}
__attribute__((weak)) void timeval_frommillis(void) {}
__attribute__((weak)) void timeval_tomicros(void) {}
__attribute__((weak)) void timeval_tomillis(void) {}
__attribute__((weak)) void timeval_toseconds(void) {}
__attribute__((weak)) void tolower_l(void) {}
__attribute__((weak)) void toupper_l(void) {}
__attribute__((weak)) void towlower_l(void) {}
__attribute__((weak)) void towupper_l(void) {}
__attribute__((weak)) void wcscoll_l(void) {}
__attribute__((weak)) void wcstoll(void) {}
__attribute__((weak)) void wcstoull(void) {}
__attribute__((weak)) void wcsxfrm_l(void) {}

/* System Calls (24 symbols) */
__attribute__((weak)) long sys_clock_gettime_monotonic_nt(void) { return -1; }
__attribute__((weak)) long sys_close_range(void) { return -1; }
__attribute__((weak)) long sys_closesocket_nt(void) { return -1; }
__attribute__((weak)) long sys_execve_nt(void) { return -1; }
__attribute__((weak)) long sys_faccessat2(void) { return -1; }
__attribute__((weak)) long sys_fchmodat_linux(void) { return -1; }
__attribute__((weak)) long sys_getpriority_nt(void) { return -1; }
__attribute__((weak)) long sys_getrandom_metal(void) { return -1; }
__attribute__((weak)) long sys_getrusage(void) { return -1; }
__attribute__((weak)) long sys_ioprio_set(void) { return -1; }
__attribute__((weak)) long sys_memfd_create(void) { return -1; }
__attribute__((weak)) long sys_pivot_root(void) { return -1; }
__attribute__((weak)) long sys_posix_openpt(void) { return -1; }
__attribute__((weak)) long sys_readlinkat(void) { return -1; }
__attribute__((weak)) long sys_setpriority(void) { return -1; }
__attribute__((weak)) long sys_setpriority_nt(void) { return -1; }
__attribute__((weak)) long sys_setsockopt(void) { return -1; }
__attribute__((weak)) long sys_sigpending(void) { return -1; }
__attribute__((weak)) long sys_sigqueueinfo(void) { return -1; }
__attribute__((weak)) long sys_sigsuspend(void) { return -1; }
__attribute__((weak)) long sys_sigtimedwait(void) { return -1; }
__attribute__((weak)) long sys_sigtimedwait_nt(void) { return -1; }
__attribute__((weak)) long sys_socketpair(void) { return -1; }
__attribute__((weak)) long sys_utimensat(void) { return -1; }

/* Threading Functions (4 symbols) */
__attribute__((weak)) int DeleteProcThreadAttributeList(void) { return 0; }
__attribute__((weak)) long GetThreadTimes(void) { return 0; }
__attribute__((weak)) int InitializeProcThreadAttributeList(void) { return 0; }
__attribute__((weak)) int UpdateProcThreadAttribute(void) { return 0; }

/* Timespec Functions (2 symbols) */
__attribute__((weak)) long timespec_sleep(void) { return 0; }
__attribute__((weak)) long timespec_sleep_until(void) { return 0; }

/* Unlocked I/O Functions (15 symbols) */
__attribute__((weak)) int clearerr_unlocked(void) { return 0; }
__attribute__((weak)) int ferror_unlocked(void) { return 0; }
__attribute__((weak)) int fgets_unlocked(void* fp) { return 0; }
__attribute__((weak)) int fgetwc_unlocked(void* fp) { return 0; }
__attribute__((weak)) int fgetws_unlocked(void* fp) { return 0; }
__attribute__((weak)) int fileno_unlocked(void) { return 0; }
__attribute__((weak)) int fprintf_unlocked(void* fp, const char* s) { return 0; }
__attribute__((weak)) int fputs_unlocked(void* fp, const char* s) { return 0; }
__attribute__((weak)) int fputwc_unlocked(void* fp, const char* s) { return 0; }
__attribute__((weak)) int fputws_unlocked(void* fp, const char* s) { return 0; }
__attribute__((weak)) int fread_unlocked(void) { return 0; }
__attribute__((weak)) int fseek_unlocked(void) { return 0; }
__attribute__((weak)) int getdelim_unlocked(void* fp) { return 0; }
__attribute__((weak)) int ungetc_unlocked(void* fp) { return 0; }
__attribute__((weak)) int ungetwc_unlocked(void* fp) { return 0; }

/* Win32 Import Pointers (19 symbols) */
__attribute__((weak)) void* __imp_AddVectoredExceptionHandler = NULL;
__attribute__((weak)) void* __imp_CreateDirectoryW = NULL;
__attribute__((weak)) void* __imp_CreateFileMappingW = NULL;
__attribute__((weak)) void* __imp_CreateProcessW = NULL;
__attribute__((weak)) void* __imp_FindFirstFileW = NULL;
__attribute__((weak)) void* __imp_FreeEnvironmentStringsW = NULL;
__attribute__((weak)) void* __imp_GetCommandLineW = NULL;
__attribute__((weak)) void* __imp_GetConsoleMode = NULL;
__attribute__((weak)) void* __imp_GetCurrentDirectoryW = NULL;
__attribute__((weak)) void* __imp_GetCurrentProcessId = NULL;
__attribute__((weak)) void* __imp_GetEnvironmentStringsW = NULL;
__attribute__((weak)) void* __imp_GetFileAttributesW = NULL;
__attribute__((weak)) void* __imp_GetModuleHandleA = NULL;
__attribute__((weak)) void* __imp_GetProcAddress = NULL;
__attribute__((weak)) void* __imp_MapViewOfFileEx = NULL;
__attribute__((weak)) void* __imp_SetConsoleMode = NULL;
__attribute__((weak)) void* __imp_SetConsoleOutputCP = NULL;
__attribute__((weak)) void* __imp_SetEnvironmentVariableW = NULL;
__attribute__((weak)) void* __imp_SetFilePointer = NULL;

/* End of auto-generated stubs (444 total) */
