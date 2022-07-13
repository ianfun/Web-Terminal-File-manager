void DeleteDirectoryInternal(LPCWSTR dir) {
	WIN32_FIND_DATAW ffd;
	std::wstring tmp{ dir };
	tmp += L"\\*";
	HANDLE hFind = FindFirstFileW(tmp.data(), &ffd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if ((ffd.cFileName[0] == L'.' && ffd.cFileName[1] == L'\0') || (ffd.cFileName[0] == L'.' && ffd.cFileName[1] == L'.' && ffd.cFileName[2] == '\0')) {

				}
				else {
					tmp = dir;
					tmp += L"\\";
					tmp += ffd.cFileName;
					log_fmt("[info] DeleteDirectoryInternal %ws\n", tmp.data());
					DeleteDirectoryInternal(tmp.data());
				}
			}
			else {
				tmp = dir;
				tmp += L"\\";
				tmp += ffd.cFileName;
				
					HANDLE hFile = CreateFileW(tmp.data(),
						DELETE,
						0,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_DIRECTORY | FILE_FLAG_BACKUP_SEMANTICS,
						NULL);
					if (hFile == INVALID_HANDLE_VALUE) {
						log_fmt("[error] CreateFile(DELETE) at %ws, err=%d\n", tmp.data(), GetLastError());
					}
					else {
						FILE_DISPOSITION_INFO d = { TRUE };
#pragma warning(push)
#pragma warning(disable: 6001)
						BOOL ok = SetFileInformationByHandle(hFile, FileDispositionInfo, &d, sizeof d);
#pragma warning(pop)
						if (ok) {
							log_fmt("[info] Delete %ws\n", tmp.data());
						}
						else {
							log_fmt("[error] SetFileInformationByHandle(FileDispositionInfo) err=%d\n", GetLastError());
						}
						CloseHandle(hFile);
					}
				
			}
		} while (FindNextFileW(hFind, &ffd));
		RemoveDirectoryW(dir);
		log_fmt("[info] RemoveDirectory() %ws\n", dir);
	}
}
inline int DeleteDirectory(LPCWSTR path) {
	DeleteDirectoryInternal(path);
	int err = GetLastError();
	return err==ERROR_NO_MORE_FILES?0:err;
}
int RenameFile(LPCWSTR oldname, LPCWSTR newname, DWORD newnameLen) {
	HANDLE hFile = CreateFileW(oldname,
		DELETE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return GetLastError();

	SIZE_T infoSize = sizeof(FILE_RENAME_INFO) + ((SIZE_T)newnameLen + 1) * 2;
	FILE_RENAME_INFO* info = (FILE_RENAME_INFO*)HeapAlloc(heap, 0, infoSize);
	if (info == NULL) {
		return ERROR_OUTOFMEMORY; // no STATUS_NO_MEMORY
	}
	info->ReplaceIfExists = TRUE;
	info->RootDirectory = NULL;
	info->FileNameLength = newnameLen;
	info->Flags = FILE_RENAME_FLAG_POSIX_SEMANTICS;
	memcpy(info->FileName, newname, ((SIZE_T)newnameLen + 1) * 2);
	BOOL res = SetFileInformationByHandle(hFile, FileRenameInfo, info, (DWORD)infoSize);
	HeapFree(heap, 0, info);
	CloseHandle(hFile);
	if (res)
		return ERROR_SUCCESS;
	return GetLastError();
}

/*
* fast list directory
* see git-for-windows commit: https://github.com/git-for-windows/git/commit/b69c08c338403a3f8fd2394180664cb9f8164c78#diff-4b6d4f2af4b31f0bc3ff43eac9a2a437
*/
decltype(&NtQueryDirectoryFile) pNtQueryDirectoryFile;

void list_dir_entry(IOCP* ctx)
{
	NTSTATUS status;
	constexpr DWORD infoSize = 1024 * 5; // 5K
	static_assert((infoSize % 4) == 0); /*aligned to 4 bytes, NextEntryOffset*/
	ctx->state = State::ListDirRecvNextRequest;
	HANDLE hFile = CreateFileW(ctx->url, FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		goto ERR;
	}
	IO_STATUS_BLOCK b;
	{
		PFILE_NAMES_INFORMATION info = reinterpret_cast<PFILE_NAMES_INFORMATION>(HeapAlloc(heap, 0, infoSize));
		if (heap == NULL) {
			goto ERR;
		}
		PFILE_NAMES_INFORMATION ptr = info;
		status = pNtQueryDirectoryFile(hFile, NULL, 0, 0, &b, info,
			infoSize, FileNamesInformation, FALSE, NULL, FALSE);
		if (status < 0) {
			WSASend(ctx->client, &HTTP_ERR_RESPONCE::internal_server_error, 1, NULL, 0, &ctx->sendOL, NULL);
			goto CLEANUP;
		}
		WideCharToMultiByte(CP_UTF8, 0,
			ctx->url, -1,
			ctx->buf, sizeof(ctx->buf),
			NULL, NULL
		);
		ctx->sbuf = new std::string(ctx->buf);
		*ctx->sbuf += "/*\"></base><ul>";
		for (;;) {
			int len = WideCharToMultiByte(
				CP_UTF8, 0,
				ptr->FileName, ptr->FileNameLength / 2,
				ctx->buf, sizeof(ctx->buf) - 1,
				NULL, NULL
			);
			ctx->buf[len] = '\0';
			*ctx->sbuf += "<li><a href=\"";
			*ctx->sbuf += ctx->buf;
			*ctx->sbuf += "\">";
			*ctx->sbuf += ctx->buf;
			*ctx->sbuf += "</a></li>";
			assert(len > 0);
			if (ptr->NextEntryOffset == 0) {
				status = pNtQueryDirectoryFile(hFile, NULL, 0, 0, &b, info,
					infoSize, FileNamesInformation, FALSE, NULL, FALSE);
				if (status < 0) {
					if (status != STATUS_NO_MORE_FILES) {
						log_puts("[error] NtQueryDirectoryFile failed on step 2");
					}
					*ctx->sbuf += "</ul></body></html>";
					break;
				}
				ptr = info;
				continue;
			}
			ptr = reinterpret_cast<PFILE_NAMES_INFORMATION>((PBYTE)ptr + ptr->NextEntryOffset);
		}
#define LISTDIR_FIRST "<!DOCTYPE html><html><head><script src=\"/upload.js\"></script><title>Directory Listing For</title></head>"\
							"<body><h1>Directory listing for</h1>"\
							"<div id=\"upload\"><input type=\"file\" id=\"f\" multiple><button onclick=\"fsubmit()\">Upload</button></div>"\
							"<base href=\"/"
		{
			int n = snprintf(
				ctx->buf,
				sizeof(ctx->buf),
				"HTTP/1.1 200 OK\r\n"
				"Content-Type: text/html; charset=utf-8\r\n"
				"Connection: %s\r\n"
				"Content-Length: %zu\r\n"
				"\r\n"
				LISTDIR_FIRST,
				ctx->keepalive ? "keep-alive" : "close",
				ctx->sbuf->length() + cstrlen(LISTDIR_FIRST)
			);
			ctx->sendBuf[0].buf = ctx->buf;
			ctx->sendBuf[0].len = (DWORD)n;
			ctx->sendBuf[1].buf = (CHAR*)ctx->sbuf->c_str();
			ctx->sendBuf[1].len = (DWORD)ctx->sbuf->size();
			WSASend(ctx->client, ctx->sendBuf, 2, NULL, 0, &ctx->sendOL, NULL);
		}
	CLEANUP:
		HeapFree(heap, 0, info);
		CloseHandle(hFile); 
		return;
	}
ERR:
	ctx->state = State::AfterSendHTML;
	WSASend(ctx->client, &HTTP_ERR_RESPONCE::internal_server_error, 1, NULL, 0, &ctx->sendOL, NULL);
}


void list_dir_entry_ex(IOCP* ctx)
{
	NTSTATUS status;
	constexpr DWORD infoSize = 10240;
	static_assert((infoSize % 4) == 0); /*aligned to 4 bytes, NextEntryOffset*/
	ctx->state = State::ListDirRecvNextRequest;
	HANDLE hFile = CreateFileW(ctx->url, FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		goto ERR;
	}
	IO_STATUS_BLOCK b;
	{
		PFILE_DIRECTORY_INFORMATION info = reinterpret_cast<PFILE_DIRECTORY_INFORMATION>(HeapAlloc(heap, 0, infoSize));
		if (heap == NULL) {
			goto ERR;
		}
		PFILE_DIRECTORY_INFORMATION ptr = info;
		status = pNtQueryDirectoryFile(hFile, NULL, 0, 0, &b, info,
			infoSize, FileDirectoryInformation, FALSE, NULL, FALSE);
		if (status < 0) {
			WSASend(ctx->client, &HTTP_ERR_RESPONCE::internal_server_error, 1, NULL, 0, &ctx->sendOL, NULL);
			goto CLEANUP;
		}
		WideCharToMultiByte(CP_UTF8, 0,
			ctx->url, -1,
			ctx->buf, sizeof(ctx->buf),
			NULL, NULL
		);
		ctx->sbuf = new std::string(ctx->buf);
		*ctx->sbuf += "/*\"><d d=\"";
		*ctx->sbuf += computer_name;
		*ctx->sbuf += "\">";
		for (;;) {
			sprintf(ctx->buf, "<z z=\"%llu;%llu;%lu\">",ptr->EndOfFile.QuadPart, ptr->LastWriteTime.QuadPart, ptr->FileAttributes);
			*ctx->sbuf += ctx->buf;
			int len = WideCharToMultiByte(
				CP_UTF8, 0,
				ptr->FileName, ptr->FileNameLength >> 1,
				ctx->buf, sizeof(ctx->buf) - 1,
				NULL, NULL
			);
			
			ctx->buf[len] = '\0';
			*ctx->sbuf += ctx->buf;
			*ctx->sbuf += "</z>";
			assert(len > 0);
			if (ptr->NextEntryOffset == 0) {
				status = pNtQueryDirectoryFile(hFile, NULL, 0, 0, &b, info,
					infoSize, FileDirectoryInformation, FALSE, NULL, FALSE);
				if (status < 0) {
					if (status != STATUS_NO_MORE_FILES) {
						log_puts("[error] NtQueryDirectoryFile failed on step 2");
					}
					*ctx->sbuf += "</d></body></html>";
					break;
				}
				ptr = info;
				continue;
			}
			ptr = reinterpret_cast<PFILE_DIRECTORY_INFORMATION>((PBYTE)ptr + ptr->NextEntryOffset);
		}
#define LISTDIR_FIRST_EX "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link rel=\"stylesheet\" href=\"/folderattr.css\"><script src=\"/upload.js\"></script><title>Directory Listing For</title></head>"\
							"<body><h1></h1>"\
							"<div id=\"upload\"><input type=\"file\" id=\"f\" multiple><button onclick=\"fsubmit()\">Upload</button></div>"\
							"<base href=\"/"
		{
			constexpr LPCSTR fmt = "HTTP/1.1 200 OK\r\n"
				"Content-Type: text/html; charset=utf-8\r\n"
				"Connection: %s\r\n"
				"Content-Length: %zu\r\n"
				"\r\n%s";
			static_assert(sizeof(ctx->buf)>=(cstrlen(fmt) + cstrlen(LISTDIR_FIRST_EX)+7));
			int n = sprintf(
				ctx->buf,
				fmt,
				ctx->keepalive ? "keep-alive" : "close",
				ctx->sbuf->length() + cstrlen(LISTDIR_FIRST_EX),
				LISTDIR_FIRST_EX
			);
			ctx->sendBuf[0].buf = ctx->buf;
			ctx->sendBuf[0].len = (DWORD)n;
			ctx->sendBuf[1].buf = (CHAR*)ctx->sbuf->c_str();
			ctx->sendBuf[1].len = (DWORD)ctx->sbuf->size();
			WSASend(ctx->client, ctx->sendBuf, 2, NULL, 0, &ctx->sendOL, NULL);
		}
	CLEANUP:
		HeapFree(heap, 0, info);
		CloseHandle(hFile);
		return;
	}
ERR:
	ctx->state = State::AfterSendHTML;
	WSASend(ctx->client, &HTTP_ERR_RESPONCE::internal_server_error, 1, NULL, 0, &ctx->sendOL, NULL);
}

