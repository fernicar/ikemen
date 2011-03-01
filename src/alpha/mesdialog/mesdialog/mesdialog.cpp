
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"zlib1.lib")

#include <windows.h>
#include <process.h>
#include <stdint.h>
#include <comdef.h>
#include <zlib.h>
static const unsigned char ASCII_FLAG  = 0x01; /* bit 0 set: file probably ascii text */
static const unsigned char HEAD_CRC    = 0x02; /* bit 1 set: header CRC present */
static const unsigned char EXTRA_FIELD = 0x04; /* bit 2 set: extra field present */
static const unsigned char ORIG_NAME   = 0x08; /* bit 3 set: original file name present */
static const unsigned char COMMENT     = 0x10; /* bit 4 set: file comment present */
static const unsigned char RESERVED    = 0xE0; /* bits 5..7: reserved */
#include "LockSingler.h"


void* (__stdcall *sszrefnewfunc)(intptr_t);
void (__stdcall *sszrefdeletefunc)(void*);

#include "../../../dll/ssz/ssz/typeid.h"
#include "../../../dll/ssz/ssz/arrayandref.hpp"
#include "../../../dll/ssz/ssz/pluginutil.hpp"

HINSTANCE ghInstance;
LockSingler g_mtx;
std::wstring g_sharedstring;

BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD fdwReason,LPVOID lpvReserved)
{
	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		ghInstance = hinstDLL;
		g_sharedstring = L'\0';
		break;
	case DLL_PROCESS_DETACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

TUserFunc(bool, YesNo, Reference r)
{
	std::wstring str;
	pu->ReftoWstr(str, r);
	return MessageBox(NULL, str.c_str(), L"メッセージ", MB_YESNO) == IDYES;
}

template<typename T> std::wstring XToStr(T x)
{
	_variant_t comvar;
	comvar = x;
	return (LPTSTR)(_bstr_t)comvar;
}

TUserFunc(void, DoubleToStr, double dbl, Reference *r)
{
	std::wstring str = XToStr(dbl);
	int foo = -1;
	for(int i = 0; i < (int)str.size(); i++){
		if(str[i] == L',') str[i] = L'.';
		if(str[i] == L'.'){
			foo = 0;
		}else if((str[i] == L'E' || str[i] == L'e') && foo < 0){
			foo = i;
		}
	}
	if(foo > 0){
		std::wstring tmp;
		tmp.append(str.data(), str.data()+foo);
		tmp += L".0";
		tmp.append(str.data()+foo, str.data()+str.size());
		str = tmp;
	}
	pu->SetSSZFunc();
	pu->WstrtoRef(*r, str);
}

TUserFunc(void, VeryUnsafeCopy, intptr_t size, void *src, void *dst)
{
	memcpy(dst, src, size);
}

TUserFunc(bool, GetClipboardStr, Reference *r)
{
	pu->SetSSZFunc();
	HANDLE hText;
	wchar_t *pText;
	OpenClipboard(NULL);
	hText = GetClipboardData(CF_UNICODETEXT);
	if(hText == NULL) return false;
	pText = (wchar_t*)GlobalLock(hText);
	pu->WstrtoRef(*r, pText);
	GlobalUnlock(hText);
	CloseClipboard();
	return true;
}

TUserFunc(intptr_t, TazyuuCheck, Reference name)
{
	std::wstring tmp;
	HANDLE hMutex;
	SECURITY_ATTRIBUTES securityatt;
	pu->ReftoWstr(tmp, name);
	securityatt.nLength = sizeof(securityatt);
	securityatt.lpSecurityDescriptor = NULL;
	securityatt.bInheritHandle = FALSE;
	hMutex = CreateMutex(&securityatt, FALSE, tmp.c_str());
	if(GetLastError() == ERROR_ALREADY_EXISTS){
		CloseHandle(hMutex);
		hMutex = NULL;
	}
	return (intptr_t)hMutex;
}

TUserFunc(void, CloseTazyuuHandle, intptr_t mutex)
{
    ReleaseMutex((HANDLE)mutex);
    CloseHandle((HANDLE)mutex);
}

TUserFunc(void, GetInifileString, Reference* pstr, Reference def, Reference key, Reference app, Reference file)
{
	pu->SetSSZFunc();
	std::wstring tmp1, tmp2, tmp3, tmp4, tmp5;
	wchar_t* pws;
	pu->ReftoWstr(tmp1, file);
	pws = _wfullpath(NULL, tmp1.c_str(), 0);
	if(pws != NULL){
		tmp1 = pws;
		free(pws);
	}
	pu->ReftoWstr(tmp2, app);
	pu->ReftoWstr(tmp3, key);
	pu->ReftoWstr(tmp4, def);
	tmp5.resize(256);
	GetPrivateProfileString(tmp2.c_str(), tmp3.c_str(), tmp4.c_str(),
		(wchar_t*)tmp5.data(), tmp5.size(), tmp1.c_str());
	pu->WstrtoRef(*pstr, tmp5.c_str());
}

TUserFunc(int32_t, GetInifileInt, int32_t def, Reference key, Reference app, Reference file)
{
	std::wstring tmp1, tmp2, tmp3;
	wchar_t* pws;
	pu->ReftoWstr(tmp1, file);
	pws = _wfullpath(NULL, tmp1.c_str(), 0);
	if(pws != NULL){
		tmp1 = pws;
		free(pws);
	}
	pu->ReftoWstr(tmp2, app);
	pu->ReftoWstr(tmp3, key);
	return GetPrivateProfileInt(tmp2.c_str(), tmp3.c_str(), def, tmp1.c_str());
}

TUserFunc(bool, WriteInifileString, Reference str, Reference key, Reference app, Reference file)
{
	std::wstring tmp1, tmp2, tmp3, tmp4;
	wchar_t* pws;
	pu->ReftoWstr(tmp1, file);
	pws = _wfullpath(NULL, tmp1.c_str(), 0);
	if(pws != NULL){
		tmp1 = pws;
		free(pws);
	}
	pu->ReftoWstr(tmp2, app);
	pu->ReftoWstr(tmp3, key);
	pu->ReftoWstr(tmp4, str);
	return WritePrivateProfileString(tmp2.c_str(), tmp3.c_str(), tmp4.c_str(), tmp1.c_str()) != 0;
}


int HeadCheck(const char *const buf, intptr_t len)
{
	int i = 0;
	if(len < 10 || buf[i++] != '\x1f' || buf[i++] != '\x8b'){
		return -1;
	}
	int method = buf[i++];
	int flags  = buf[i++];
	if(method != Z_DEFLATED || (flags & RESERVED) != 0){
		return -1;
	}
	i += 6;
	if(flags & EXTRA_FIELD){
		int len = (unsigned char)buf[i++];
		len += ((int)(unsigned char)buf[i++] << 8);
		i += len;
	}
	if(flags & ORIG_NAME){
		while(i < len) if(buf[i++] == 0) break;
	}
	if(flags & COMMENT){
		while(i < len) if(buf[i++] == 0) break;
	}
	if(flags & HEAD_CRC){
		i += 2;
	}
	if(i > len) return -1;
	return i;
}

TUserFunc(bool, UnCompress, Reference src, Reference *dst)
{
	pu->SetSSZFunc();
	z_stream z;
	intptr_t hsize;
	char outbuf[4096];
	int status;
	std::string mainbuf;
	if(src.len() == 0) return false;
	if((hsize = HeadCheck((char*)src.atpos(), src.len())) < 0) hsize = 0;
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;
	z.next_in = Z_NULL;
	z.avail_in = 0;
	if(hsize > 0){
		if(inflateInit2(&z, -MAX_WBITS) != Z_OK) return false;
	}else{
		if(inflateInit(&z) != Z_OK) return false;
	}
	z.next_in = (BYTE *)src.atpos()+hsize;
	z.avail_in = (uInt)(src.len()-hsize);
	z.next_out = (BYTE *)outbuf; 
	z.avail_out = (uInt)sizeof(outbuf);
	do{
		if(z.avail_out == 0){
			mainbuf.append(outbuf, sizeof(outbuf));
			z.next_out = (Bytef*)outbuf;
			z.avail_out = sizeof(outbuf);
		}
		status = inflate(&z, Z_NO_FLUSH);
	}while(status == Z_OK);
	if(status != Z_STREAM_END){
		inflateEnd(&z);
		return false;
	}
	mainbuf.append(outbuf, sizeof(outbuf) - z.avail_out);
	inflateEnd(&z);
	dst->releaseanddelete();
	dst->refnew(mainbuf.size(), sizeof(int8_t));
	if(dst->len() > 0) memcpy(dst->atpos(), mainbuf.data(), mainbuf.size());
	return true;
}

TUserFunc(void, UbytesToStr, Reference src, Reference *dst, UINT cp)
{
	pu->SetSSZFunc();
	dst->releaseanddelete();
	if(src.len() == 0) return;
	dst->refnew(MultiByteToWideChar(cp, 0, (LPCSTR)src.atpos(), (int)src.len(), NULL, 0), sizeof(wchar_t));
	if(dst->len() > 0) MultiByteToWideChar(cp, 0, (LPCSTR)src.atpos(), (int)src.len(), (wchar_t*)dst->atpos(), (int)dst->len()/sizeof(wchar_t));
}

TUserFunc(void, StrToUbytes, Reference src, Reference *dst, UINT cp)
{
	pu->SetSSZFunc();
	dst->releaseanddelete();
	if(src.len() == 0) return;
	dst->refnew(WideCharToMultiByte(cp, 0, (wchar_t*)src.atpos(), (int)src.len()/sizeof(wchar_t), NULL, 0, NULL, NULL), sizeof(uint8_t));
	if(dst->len() > 0) WideCharToMultiByte(cp, 0, (wchar_t*)src.atpos(), (int)src.len()/sizeof(wchar_t), (LPSTR)dst->atpos(), (int)dst->len(), NULL, NULL);
}

TUserFunc(void, AsciiToLocal, Reference src, Reference *dst)
{
	pu->SetSSZFunc();
	dst->releaseanddelete();
	if(src.len() == 0) return;
	std::string tmp;
	intptr_t i, l = src.len()/sizeof(wchar_t);
	for(i = 0; i < l; i++) tmp += (char)((wchar_t*)src.atpos())[i];
	dst->refnew(MultiByteToWideChar(CP_THREAD_ACP, 0, tmp.data(), tmp.size(), NULL, 0), sizeof(wchar_t));
	if(dst->len() > 0) MultiByteToWideChar(CP_THREAD_ACP, 0, tmp.data(), tmp.size(), (wchar_t*)dst->atpos(), (int)dst->len()/sizeof(wchar_t));
}

TUserFunc(void, SetSharedString, Reference str)
{
	AutoLocker al(&g_mtx);
	pu->ReftoWstr(g_sharedstring, str);
}

TUserFunc(void, GetSharedString, Reference *str)
{
	pu->SetSSZFunc();
	AutoLocker al(&g_mtx);
	pu->WstrtoRef(*str, g_sharedstring.c_str());
}









#include "mydll.h"

// Data Structures
typedef struct tagXFERBUFFER2
{
	wchar_t* lpszTitle;
	LPTSTR* lpszBuffer;
	int* length;
}XFERBUFFER2;
//Prototyping
BOOL WINAPI InputBoxDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
int WINAPI WEP(int nShutDownFlag);
/*******************************************/ 
/* THE FOLLOWING FUNCTION  (WEP) IS    */ 
/* NEEDED FOR EVERY .DLL                   */ 
/*******************************************/ 
int WINAPI WEP(int nShutDownFlag)
{
	return 1;
}
/*******************************************/ 
/* THE FOLLOWING FUNCTION  (MyInputBox) IS*/ 
/* WHAT THE FOXPRO .PRG CALLS*/ 
/*******************************************/ 
/*******************************************/ 
INT_PTR WINAPI MyInputBox(HWND hWndParent, const wchar_t*  lpszTitle, LPTSTR *Buffer, int *Length)
{
	DLGPROC lpfnInputBoxDlgProc;
	XFERBUFFER2 XferBuffer;
	INT_PTR Result;
	XferBuffer.lpszTitle = (wchar_t*)lpszTitle;
	XferBuffer.lpszBuffer = Buffer;
	XferBuffer.length = Length;
	lpfnInputBoxDlgProc = (DLGPROC)MakeProcInstance((FARPROC)InputBoxDlgProc,
		ghInstance);
	Result=DialogBoxParam(ghInstance,L"INPUTDIALOG",
		hWndParent,lpfnInputBoxDlgProc,(LPARAM)&XferBuffer);
	FreeProcInstance((FARPROC)lpfnInputBoxDlgProc);
	return Result;
}
/*******************************************/ 
BOOL WINAPI InputBoxDlgProc(HWND hDlg, UINT message,WPARAM wParam, LPARAM lParam)
{
	static XFERBUFFER2 *XferBuffer;
	switch(message)
	{
	case WM_INITDIALOG :
		{
			XferBuffer = (XFERBUFFER2*)lParam;
			SetWindowText(hDlg, XferBuffer->lpszTitle);
			return TRUE; 
		}
	case WM_COMMAND :
		{
			switch(wParam)
			{
			case IDOK :
				{
					int NumChars;
					*XferBuffer->length = GetWindowTextLength(GetDlgItem(hDlg,IDD_EDIT));
					*XferBuffer->lpszBuffer = new wchar_t[*XferBuffer->length + 1];
					NumChars=GetDlgItemText(hDlg,IDD_EDIT,
						*XferBuffer->lpszBuffer,
						*XferBuffer->length + 1);
					EndDialog(hDlg,NumChars);
					break; 
				}
			case IDCANCEL :
				{
					EndDialog(hDlg, 0);
					break;
				}
			}
		}
	}
	return FALSE;
}

TUserFunc(void, InputStr, Reference *pr, Reference title)
{
	pu->SetSSZFunc();
	std::wstring tit;
	pu->ReftoWstr(tit, title);
	wchar_t* str;
	int len;
	len = -1;
	str = NULL;
	MyInputBox(NULL, tit.c_str(), &str, &len);
	if(len >= 0){
		pu->WstrtoRef(*pr, str);
	}else{
		pr->releaseanddelete();
	}
}
