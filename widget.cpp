#include "widget.h"
#include "ui_widget.h"
#include <QDebug>
#include <QFileDialog>
#include <windows.h>
#include <TlHelp32.h>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{


    ui->setupUi(this);
    enumProcess();
    connect(this,&Widget::refreshProcTab,this,&Widget::enumProcess);
    upRole(); // 进程提权
    ui->processTab->setColumnWidth(0,100);
    ui->dllTab->setColumnWidth(0,100);
    ui->dllTab->setColumnWidth(1,200);
}

uint Widget::getPid()
{
    int row = ui->processTab->currentRow();
    uint pid = ui->processTab->item(row,1)->text().toUInt();
    return pid;
}

// 枚举进程
void Widget::enumProcess()
{
    // 清空表格
    int rowCount = ui->processTab->rowCount();
    for( int i = 0; i < rowCount; i++ )
    {
        ui->processTab->removeRow(0);
    }

    HANDLE snapHandele = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,NULL);
    if( INVALID_HANDLE_VALUE == snapHandele)
    {
        qDebug() << "CreateToolhelp32Snapshot error" ;
        return;
    }

    PROCESSENTRY32 entry = {0};
    entry.dwSize = sizeof(entry);// 长度必须赋值
    BOOL ret = Process32First(snapHandele,&entry);
    int i = 0;
    while (ret) {
        QString exeFile = QString::fromWCharArray(entry.szExeFile);
        ui->processTab->insertRow(i);
        ui->processTab->setItem(i,0,new QTableWidgetItem(exeFile));
        ui->processTab->setItem(i,1,new QTableWidgetItem(QString("%1").arg(entry.th32ProcessID)));
        i++;
        ret = Process32Next(snapHandele,&entry);
    }
    CloseHandle(snapHandele);
}
Widget::~Widget()
{
    delete ui;
}

void Widget::on_pushButton_5_clicked()
{
    // 退出
    this->close();
}

// 提升当前进程权限至SeDebugPrivilege
// 笔者在win10中没有提权成功！
void Widget::upRole()
{
//    HWND curWnd = (HWND) winId();
//    DWORD curPid;
//    GetWindowThreadProcessId(curWnd,&curPid);
//    HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS,FALSE,curPid);

    HANDLE pHandle = GetCurrentProcess();
    HANDLE tHandle;
    BOOL ret = OpenProcessToken(pHandle,TOKEN_ALL_ACCESS,&tHandle); // 打开当前进程的访问令牌
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    ret = LookupPrivilegeValue(NULL,SE_DEBUG_NAME,&tp.Privileges[0].Luid); // 获取描述权限的LUID，SE_DEBUG_NAME调试权限名字
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    ret = AdjustTokenPrivileges(tHandle,FALSE,&tp,sizeof(tp),NULL,NULL); // 调整访问令牌的权限
    CloseHandle(tHandle);
}

// 创建进程
void Widget::on_pushButton_6_clicked()
{

    QFileDialog *fileDialog = new QFileDialog(this);
    fileDialog->setWindowTitle(tr("打开可执行文件"));
    fileDialog->setDirectory(".");
    fileDialog->setNameFilter(tr("可执行文件(*.exe)"));
    fileDialog->setViewMode(QFileDialog::Detail);
    QStringList fileNames;
    if(fileDialog->exec())
    {
        QString fileName;
        fileNames =  fileDialog->selectedFiles();
    }else
    {
        return;
    }
    QString exePath = fileNames[0];
    qDebug() << exePath;

    const char* path = exePath.toStdString().c_str();
    STARTUPINFOA startInfo = {0};
    startInfo.cb = sizeof(startInfo);
    PROCESS_INFORMATION processInfo = {0};
    // startInfo 和 processInfo必须初始化
    BOOL ret = CreateProcessA(path,NULL,NULL,NULL,false,NULL
                  ,NULL,NULL,&startInfo,&processInfo);
    if( ret )
    {
        qDebug() << "processId = " << processInfo.dwProcessId << ",threadId = "<< processInfo.dwThreadId ;
        CloseHandle( processInfo.hProcess );
        CloseHandle( processInfo.hThread );
        emit refreshProcTab();
    }else
    {
        qDebug() << "创建进程失败" ;
    }

}

// 结束进程
void Widget::on_pushButton_clicked()
{

    int row = ui->processTab->currentRow();
    uint pid = getPid();

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS,FALSE,pid); // 获取进程句柄
    if( hProc == NULL)
    {
        qDebug() << "OpenProcess error ";
        return;
    }
    BOOL ret = TerminateProcess(hProc,0); // 强制进程退出
    if(ret == FALSE)
    {
        qDebug() << "TerminateProcess error ";
        return ;
    }
    ui->processTab->removeRow(row);
    CloseHandle(hProc);
}

// 停止进程，就是将进程中的所有线程挂起
void Widget::on_pushButton_2_clicked()
{
    uint pid = getPid();
    HANDLE snapHandele = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,NULL);
    if( INVALID_HANDLE_VALUE == snapHandele)
    {
        qDebug() << "CreateToolhelp32Snapshot error" ;
        return;
    }
    THREADENTRY32 entry = {0};
    entry.dwSize = sizeof(entry);
    BOOL ret = Thread32First(snapHandele,&entry);
    while( ret )
    {
        if( entry.th32OwnerProcessID == pid)
        {

            HANDLE tHandle = OpenThread(THREAD_ALL_ACCESS,FALSE,entry.th32ThreadID);
            if( tHandle == NULL)
            {
                qDebug() << "OpenThread error，threadId = " << entry.th32ThreadID;
            }
            else
            {
                DWORD ret = SuspendThread(tHandle);
                if( ret == -1)
                {
                    qDebug() << "SuspendThread error";
                }else
                {
                    qDebug() << "SuspendThread success";
                }
                CloseHandle(tHandle);
            }
        }
        ret = Thread32Next(snapHandele,&entry);
    }
    CloseHandle(snapHandele);
}

 // 恢复进程，就是将进程中的所有线程恢复
void Widget::on_pushButton_3_clicked()
{
    uint pid = getPid();
    HANDLE snapHandele = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,NULL);
    if( INVALID_HANDLE_VALUE == snapHandele)
    {
        qDebug() << "CreateToolhelp32Snapshot error" ;
        return;
    }
    THREADENTRY32 entry = {0};
    entry.dwSize = sizeof(entry);
    BOOL ret = Thread32First(snapHandele,&entry);
    while( ret )
    {
        if( entry.th32OwnerProcessID == pid)
        {

            HANDLE tHandle = OpenThread(THREAD_ALL_ACCESS,FALSE,entry.th32ThreadID);
            if( tHandle == NULL)
            {
                qDebug() << "OpenThread error，threadId = " << entry.th32ThreadID;
            }
            else
            {
                DWORD ret = ResumeThread(tHandle);
                if( ret == -1)
                {
                    qDebug() << "SuspendThread error";
                }else
                {
                    qDebug() << "ResumeThread success";
                }
                CloseHandle(tHandle);
            }
        }
        ret = Thread32Next(snapHandele,&entry);
    }
}

// 查看进程的DLL
// 查看某些系统进程的DLL是不行的，调用CreateToolhelp32Snapshot直接失败
// 必须提权，提权操作在upRole函数中
void Widget::on_pushButton_4_clicked()
{
    // 清空表格
    int rowCount = ui->dllTab->rowCount();
    for( int i = 0; i < rowCount; i++ )
    {
        ui->dllTab->removeRow(0);
    }
    uint pid = getPid();
    qDebug() << "pid = " << pid;


    HANDLE snapHandele = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE ,pid);
    if( INVALID_HANDLE_VALUE == snapHandele)
    {
        qDebug() << "CreateToolhelp32Snapshot error" ;
        return;
    }
    MODULEENTRY32 entry = {0};
    entry.dwSize = sizeof(entry);// 长度必须赋值
    BOOL ret = Module32First(snapHandele,&entry);
    int i = 0;
    HMODULE a = NULL;
    while (ret) {
        QString dllFile = QString::fromWCharArray(entry.szModule);
        QString dllPath = QString::fromWCharArray(entry.szExePath);
        ui->dllTab->insertRow(i);
        ui->dllTab->setItem(i,0,new QTableWidgetItem(dllFile));
        ui->dllTab->setItem(i,1,new QTableWidgetItem(QString("%1").arg(dllPath)));
        i++;
        ret = Module32Next(snapHandele,&entry);
    }
    CloseHandle(snapHandele);
}
