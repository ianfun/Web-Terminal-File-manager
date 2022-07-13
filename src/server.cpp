static struct {
	SOCKET server;
	IOCP* currentCtx;
} acceptIOCP{};
LPCWSTR urlToPath(IOCP* ctx);
void processRequest(IOCP* ctx, DWORD dwbytes);
void processIOCP(IOCP* ctx, OVERLAPPED* ol, DWORD dwbytes);
void parse_keepalive(IOCP* ctx) {
	if (ctx->firstCon) {
		ctx->firstCon = false;
		int iResult = 0;
		iResult = setsockopt(ctx->client, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char*)&acceptIOCP.server, sizeof(acceptIOCP.server));
		auto connection = ctx->p.headers.find("Connection");
		if (connection != ctx->p.headers.end()) {
			if (connection->second == "keep-alive" || connection->second == "Keep-Alive") {
				ctx->keepalive = true;
				DWORD yes = TRUE;
				int success = setsockopt(ctx->client, SOL_SOCKET, SO_KEEPALIVE, (char*)&yes, sizeof(yes));
				if (success != 0) {
					log_fmt("setsockopt(SO_KEEPALIVE) failed on socket %lld\n", ctx->client);
				}
				auto keepalive = ctx->p.headers.find("Keep-Alive");
				if (keepalive == ctx->p.headers.end())
					keepalive = ctx->p.headers.find("keep-alive");
				if (keepalive != ctx->p.headers.end()) {
					auto s = keepalive->second.data();
					auto timeouts = StrStrA(s, "timeout");
					if (timeouts) {
						int timeout;
						int res = sscanf_s(timeouts + 7, "=%d", &timeout);
						if (res > 0) {
							log_fmt("set TCP keep alive timeout=%d\n", timeout);
							int yes = TRUE;
							res = setsockopt(ctx->client, SOL_SOCKET, TCP_KEEPIDLE, (char*)&yes, sizeof yes);
							assert(res == 0);
						}
						else {
							log_puts("Error: failed to parse keepalive seconds...");
						}
					}
				}
			}
		}
	}
	_ASSERT(_CrtCheckMemory());
}
void CloseClient(IOCP* ctx) {
	if (ctx->state != State::AfterClose) {
		ctx->state = State::AfterClose;
		(void)shutdown(ctx->client, SD_BOTH);
		if (closesocket(ctx->client) != 0) {
			assert(0);
		}
		if (ctx->sbuf) {
			ctx->sbuf->~basic_string();
		}
		if (ctx->hasp) {
			(&ctx->p)->~Parse_Data();
		}
		if (ctx->url) {
			HeapFree(heap, 0, (LPVOID)ctx->url);
		}
		if (ctx->hProcess && ctx->hProcess != INVALID_HANDLE_VALUE) {
			CloseHandle(ctx->hProcess);
		}
		if (HeapFree(heap, 0, (LPVOID)ctx) == FALSE) {
			assert(0);
		}
	}
}
int http_on_header_complete(llhttp_t* parser) {
	IOCP* ctx = (IOCP*)parser;
	parse_keepalive(ctx);
	if (parser->method == llhttp_method_t::HTTP_POST) {
		WSABUF* errBuf = &HTTP_ERR_RESPONCE::internal_server_error;
		LPCWSTR path = urlToPath(ctx);
			HANDLE hFile = CreateFileW(path,
				GENERIC_WRITE,
				FILE_SHARE_WRITE,
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
				NULL);
			log_fmt("[info] create file: %ws\n", path);
			if (hFile == INVALID_HANDLE_VALUE || CreateIoCompletionPort(hFile, iocp, (ULONG_PTR)ctx, N_THREADS)==NULL) {
				log_fmt("[warn] file handle error %lld, err=%d\n", (__int64)hFile, GetLastError());
				errBuf = &HTTP_ERR_RESPONCE::internal_server_error;
				goto BAD_REQUEST_AND_RELEASE;
			}
			ctx->hProcess = hFile;
			assert(ctx->sendOL.Offset == 0);
			assert(ctx->sendOL.OffsetHigh == 0);
			assert(ctx->sendOL.hEvent==NULL);
			ctx->state = State::POSTWaitFileData;
			return 0;
		
	BAD_REQUEST_AND_RELEASE:
		CloseClient(ctx); // quickly shutdown POST request
		return -1;
	}

	return 0;
}
int http_on_body(llhttp_t* parser, const char* at, size_t length) {
	IOCP* ctx = reinterpret_cast<IOCP*>(parser);
	if (ctx->hProcess) {
		ctx->state = State::PostWritePartFile;
		assert(ctx->sendOL.hEvent == NULL);
		WriteFile(ctx->hProcess, at, (DWORD)length, NULL, &ctx->sendOL);
	}
	else {
		log_puts("[client error] not a POST body, close client");
		CloseClient(ctx);
	}
	return 0;
}
int http_on_header_field(llhttp_t* parser, const char* at, size_t length) {
	Parse_Data* p = (Parse_Data*)parser;
	p->length = length;
	p->at = at;
	return 0;
}
int http_on_header_value(llhttp_t* parser, const char* at, size_t length) {
	Parse_Data* p = (Parse_Data*)parser;
	p->headers[std::string(p->at, p->length)] = std::string(at, length);
	return 0;
}		

int http_on_url(llhttp_t* parser, const char* at, size_t length) {
	if(length == 1){
		return 0;
	}
	IOCP* ctx = reinterpret_cast<IOCP*>(parser);
	std::string tmp{at+1, length-1};
	if (UrlUnescapeA(&tmp[0], NULL, NULL, URL_UNESCAPE_INPLACE | URL_UNESCAPE_AS_UTF8)!=S_OK){
		assert(0);
	}
	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, tmp.data(), -1, NULL, 0);
	if(len <= 0){
		log_fmt("[client error] the url is not UTF-8 encoded(%*.s)\n", (int)length, at);
		return -1;
	}
	ctx->url = (WCHAR*)HeapAlloc(heap, 0, (SIZE_T)len * 2);
	if(ctx->url == NULL){
		log_puts("[error] HeapAlloc failed!");
		return -1;
	}
	len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, tmp.data(), -1, ctx->url, len);
	assert(len > 0);
	log_fmt("[request](method=%s, url=%ws)\n", llhttp_method_name((llhttp_method)ctx->p.parser.method), ctx->url);
	return 0;
}
void processIOCP(IOCP* ctx, OVERLAPPED* ol, DWORD dwbytes) {
	switch (ctx->state) {
	case State::AfterRecv:
	{
		processRequest(ctx, dwbytes);
	}break;
	case State::AfterHandShake:
	{
		ctx->state = State::WebSocketConnecting;
		WCHAR name[] = L"cmd";
		BOOL res = spawn(name, ctx);
		if (res == FALSE) {
			int n = snprintf(ctx->buf + 2, sizeof(ctx->buf) - 2, "Server Error: create process failed, GetLastError()=%d", GetLastError());
			*(PWORD)ctx->buf = htons(1000);
			assert(n > 0);
			websocketWrite(ctx, ctx->buf, n + 2, &ctx->sendOL, ctx->sendBuf, Websocket::Opcode::Close);
			return;
		}
		ctx->recvBuf[0].buf = ctx->buf;
		ctx->recvBuf[0].len = 6;
		ctx->dwFlags = MSG_WAITALL;
		ctx->Reading6Bytes = true;
		WSARecv(ctx->client, ctx->recvBuf, 1, NULL, &ctx->dwFlags, &ctx->recvOL, NULL);
	}break;
	case State::ReadStaticFile:
	{
		(void)ReadFile(ctx->hProcess, ctx->buf, sizeof(ctx->buf), NULL, &ctx->recvOL);
		ctx->state = State::SendPartFile;
	}break;
	case State::SendPartFile:
	{
		*(reinterpret_cast<UINT64*>(&ctx->recvOL.Offset)) += dwbytes;
		ctx->sendBuf->len = dwbytes;
		WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
		if (*(reinterpret_cast<UINT64*>(&ctx->recvOL.Offset)) == ctx->filesize) {
			if (ctx->keepalive) {
				ctx->state = State::RecvNextRequest;
			}
			else {
				ctx->state = State::AfterSendHTML;
			}
		}
		else {
			ctx->state = State::ReadStaticFile;
		}
	}break;
	case State::RecvNextRequest:
	{
		CloseHandle(ctx->hProcess);
		ctx->dwFlags = 0;
		assert(ctx->recvBuf[0].buf == ctx->buf);
		assert(ctx->recvBuf[0].len == sizeof(ctx->buf));
		assert(ctx->recvOL.hEvent == NULL);
		ctx->recvOL.Offset = ctx->recvOL.OffsetHigh = 0;
		WSARecv(ctx->client, ctx->recvBuf, 1, NULL, &ctx->dwFlags, &ctx->recvOL, NULL);
		ctx->state = State::AfterRecv;
	}break;
	case State::ListDirRecvNextRequest:
	{
		if (ctx->keepalive) {
			ctx->sbuf->~basic_string();
			ctx->state = State::AfterRecv;
			ctx->dwFlags = 0;
			assert(ctx->recvOL.hEvent == NULL);
			assert(ctx->recvBuf[0].buf == ctx->buf);
			assert(ctx->recvBuf[0].len == sizeof(ctx->buf));
			WSARecv(ctx->client, ctx->recvBuf, 1, NULL, &ctx->dwFlags, &ctx->recvOL, NULL);
		}
		else {
			CloseClient(ctx);
		}
	}break;
	case  State::PostRecvNectRequest:{
		if (ctx->keepalive) {
			ctx->state = State::AfterRecv;
			ctx->dwFlags = 0;
			WSARecv(ctx->client, ctx->recvBuf, 1, NULL, &ctx->dwFlags, &ctx->recvOL, NULL);
		}
		else {
			CloseClient(ctx);
		}
	}break;
	case State::PostWritePartFile:
	{
		*(reinterpret_cast<UINT64*>(& ctx->sendOL.Offset)) += dwbytes;
		if (ctx->p.parser.content_length==0) {
			CloseHandle(ctx->hProcess);
			ctx->state = State::PostRecvNectRequest;
			ctx->sendOL.Offset = ctx->sendOL.OffsetHigh = 0;
			ctx->sendBuf->buf = ctx->buf;
			ctx->sendBuf->len = sprintf(ctx->buf, "HTTP/1.1 204 No Content\r\nConnection: %s\r\n\r\n", ctx->keepalive?"keep-alive":"close");;
			WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
			break;
		}
		WSARecv(ctx->client, ctx->recvBuf, 1, NULL, &ctx->dwFlags, &ctx->recvOL, NULL);
		ctx->state = State::POSTWaitFileData;
	}break;
	case State::POSTWaitFileData:
	{
		auto err = llhttp_execute(&ctx->p.parser, ctx->buf, dwbytes);
		if (err != llhttp_errno::HPE_OK) {
			assert(0);
			CloseClient(ctx);
		}
		assert(ctx->state == State::PostWritePartFile);
	}break;
	case State::WebSocketConnecting: {
		if (ol == &ctx->recvOL) {
			if (ctx->Reading6Bytes) {
				onRead6Complete(ctx);
			}
			else {
				onRecvData(ctx);
			}
		}
		else if (ol == &ctx->sendOL) {
			CloseClient(ctx);
		}
		else {
			assert(0);
		}
	}break;
	case State::AfterSendHTML: {
		CloseClient(ctx);
	}break;
	case State::AfterClose:
	default:
	{
		log_fmt("invalid state: %u\n", ctx->state);
		assert(0);
	}
	}
}
void processRequest(IOCP* ctx, DWORD dwbytes) {
	ctx->buf[dwbytes] = '\0';
	ctx->hasp = true;
	if (ctx->firstCon) {
		new(&ctx->p)Parse_Data{};
	}
	enum llhttp_errno err = llhttp_execute(&ctx->p.parser, ctx->buf, dwbytes);
	if (ctx->p.parser.method == llhttp_method_t::HTTP_POST) {
		if (ctx->state == State::POSTWaitFileData) {
			WSARecv(ctx->client, ctx->recvBuf, 1, NULL, &ctx->dwFlags, &ctx->recvOL, NULL);
		}
		return;
	}
	if (err != HPE_OK && err != HPE_PAUSED_UPGRADE) {
		log_fmt("llhttp_execute error: %s\n", llhttp_errno_name(err));
		CloseClient(ctx);
		return;
	}
	WSABUF* errBuf = &HTTP_ERR_RESPONCE::internal_server_error;
	switch (ctx->p.parser.method) {
	case llhttp_method::HTTP_GET: {
		switch (err) {
		case HPE_OK:
		{
			LPCWSTR file = urlToPath(ctx);
			HANDLE hFile = CreateFileW(file,
				GENERIC_READ,
				FILE_SHARE_READ,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
				NULL);
			if (hFile == INVALID_HANDLE_VALUE) {
				if (GetLastError() == ERROR_ACCESS_DENIED) {
					list_dir_entry_ex(ctx);
					break;
				}
				errBuf = &HTTP_ERR_RESPONCE::not_found;
				goto BAD_REQUEST_AND_RELEASE;
			}
			HANDLE r = CreateIoCompletionPort(hFile, iocp, (ULONG_PTR)ctx, N_THREADS);
			assert(r);
			const char* mine = getType(file);
			ctx->hProcess = hFile;
			LARGE_INTEGER fsize{};
			BOOL bSuccess = GetFileSizeEx(hFile, &fsize);
			ctx->filesize = (UINT64)fsize.QuadPart;
			assert(bSuccess);
			int res = snprintf(ctx->buf, sizeof(ctx->buf),
				"HTTP/1.1 200 OK\r\n"
				"Content-Type: %s\r\n"
				"Content-Length: %lld\r\n"
				"Connection: %s\r\n\r\n", mine, fsize.QuadPart, ctx->keepalive ? "keep-alive" : "close");
			assert(res > 0);
			ctx->sendBuf->buf = ctx->buf;
			ctx->sendBuf->len = (ULONG)res;
			WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
			ctx->state = State::ReadStaticFile;
		}break;
		case HPE_PAUSED_UPGRADE:
		{
			auto upgrade = ctx->p.headers.find("Upgrade");
			auto pro = ctx->p.headers.find("Sec-WebSocket-Protocol");
			if (upgrade != ctx->p.headers.end() && pro != ctx->p.headers.end()) {
				if (upgrade->second == "websocket") {
					auto ws_key = ctx->p.headers.find("Sec-WebSocket-Key");
					if (ws_key != ctx->p.headers.end()) {
						ctx->state = State::AfterHandShake;
						ws_key->second += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
						char buf[29];
						BOOL ret = HashHanshake(ws_key->second.data(), (ULONG)ws_key->second.length(), buf);
						assert(ret);
						int len;
						len = snprintf(ctx->buf, sizeof(ctx->buf),
							"HTTP/1.1 101 Switching Protocols\r\n"
							"Upgrade: WebSocket\r\n"
							"Connection: Upgrade\r\n"
							"Sec-WebSocket-Protocol: %s\r\n"
							"Sec-WebSocket-Accept: %s\r\n\r\n", pro->second.data(), buf);
						ctx->sendBuf[0].buf = ctx->buf;
						ctx->sendBuf[0].len = (ULONG)len;
						WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
						PWSTR q = StrChrIW(ctx->url, L'?');
						if (q) {
							q++;
							int n = swscanf_s(q, L"rows=%hu&cols=%hu", &ctx->coord.X, &ctx->coord.Y);
							if (n != 2) {
								goto BAD;
							}
						}else{
							goto BAD;
							
						}
						BAD:
							ctx->coord.X = 130;
							ctx->coord.Y = 70;
					}
					else {
						errBuf = &HTTP_ERR_RESPONCE::bad_request;
						goto BAD_REQUEST_AND_RELEASE;
					}
				}
				else {
					errBuf = &HTTP_ERR_RESPONCE::bad_request;
					goto BAD_REQUEST_AND_RELEASE;
				}
			}
			else {
				errBuf = &HTTP_ERR_RESPONCE::bad_request;
				goto BAD_REQUEST_AND_RELEASE;
			}
		}break;
		DEFAULT_UNREACHABLE;
		}
	}break;
	case llhttp_method::HTTP_HEAD:
	{
		if (err == llhttp_errno::HPE_OK) {
			HANDLE hFile = CreateFileW(urlToPath(ctx),
				GENERIC_READ,
				FILE_SHARE_READ,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
			if (hFile == INVALID_HANDLE_VALUE)
			{
				constexpr const char* msg = "HTTP/1.1 404 Not Found\r\n\r\n";
				ctx->sendBuf[0].buf = (CHAR*)msg;
				ctx->sendBuf[0].len = cstrlen(msg);
				ctx->state = State::AfterSendHTML;
				WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
				return;
			}
			FILETIME ftWrite;
			SYSTEMTIME stUTC;
			LARGE_INTEGER fsize;
			if (GetFileTime(hFile, NULL, NULL, &ftWrite)) {
				if (FileTimeToSystemTime(&ftWrite, &stUTC)) {
					char timebuf[30];
					if (InternetTimeFromSystemTimeA(&stUTC, INTERNET_RFC1123_FORMAT, timebuf, sizeof(timebuf))) {
						if (GetFileSizeEx(hFile, &fsize)) {
							/*
								head request: always close connection after sent header
							*/
							int len = snprintf(ctx->buf, sizeof(ctx->buf),
								"HTTP/1.1 200 OK\r\n" // no "Accept-Ranges: bytes\r\n" now
								"Last-Modified: %s\r\n"
								"Conetnt-Length: %lld\r\n"
								"Connection: close\r\n"
								"\r\n", timebuf, fsize.QuadPart);
							ctx->sendBuf[0].buf = ctx->buf;
							ctx->sendBuf[0].len = len;
							ctx->state = State::AfterSendHTML;
							WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
							return;
						}
					}
				}
			}
			errBuf = &HTTP_ERR_RESPONCE::internal_server_error;
			goto BAD_REQUEST_AND_RELEASE;
		}
		else {
			goto BAD_REQUEST_AND_RELEASE;
		}
	}break;
	case llhttp_method::HTTP_PUT:
	{
		BOOL ok = CreateDirectoryW(ctx->url, NULL);
		log_fmt("[info] create Folder %ws (%s)\n", ctx->url, ok?"ok":"failed");
		if (ok) {
			ctx->sendBuf->buf = HTTP_ERR_RESPONCE::new_dir_ok;
			ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::new_dir_ok);
		}
		else {
			switch (GetLastError()) {
			case ERROR_ALREADY_EXISTS:
				ctx->sendBuf->buf = HTTP_ERR_RESPONCE::new_dir_exisits;
				ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::new_dir_exisits);
				break;
			case ERROR_PATH_NOT_FOUND:
				ctx->sendBuf->buf = HTTP_ERR_RESPONCE::new_dir_not_found;
				ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::new_dir_not_found);
				break;
			default:
				ctx->sendBuf->buf = HTTP_ERR_RESPONCE::new_dir_err;
				ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::new_dir_err);
			}
		}
		ctx->state = State::AfterSendHTML;
		WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
	}break;
	case llhttp_method::HTTP_DELETE:
	{
		auto newName = ctx->p.headers.find("X-NewName");
		if (newName != ctx->p.headers.end()) {
			if (UrlUnescapeA(&newName->second[0], NULL, NULL, URL_UNESCAPE_INPLACE | URL_UNESCAPE_AS_UTF8)!=S_OK) {
				errBuf = &HTTP_ERR_RESPONCE::client_percent_err;
				goto BAD_REQUEST_AND_RELEASE;
			}
			int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, newName->second.data(), -1, NULL, 0);
			if (len <= 0) {
				errBuf = &HTTP_ERR_RESPONCE::client_utf8_err;
				goto BAD_REQUEST_AND_RELEASE;
			}
			WCHAR* newNameW = (WCHAR*)HeapAlloc(heap, 0, (SIZE_T)len*2);
			len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, newName->second.data(), -1, newNameW, len);
			assert(len > 0);
			int err = RenameFile(ctx->url, newNameW, len);
			log_fmt("[info] RENAME %ws to %ws(err=%d)\n", ctx->url, newNameW, err);
			HeapFree(heap, 0, newNameW);
			switch (err) {
			case ERROR_SUCCESS:
				ctx->sendBuf->buf = HTTP_ERR_RESPONCE::move_ok;
				ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::move_ok);
				break;
			case ERROR_PATH_NOT_FOUND:
				ctx->sendBuf->buf = HTTP_ERR_RESPONCE::move_not_found;
				ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::move_not_found);
				break;
			default:
				ctx->sendBuf->buf = HTTP_ERR_RESPONCE::move_err;
				ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::move_err);
				break;
			}
			ctx->state = State::AfterSendHTML;
			WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
			break;
		}
		if ((ctx->p.headers.find("X-Recursively"))!=ctx->p.headers.end()) {
			int err = DeleteDirectory(ctx->url);
			log_fmt("[info] DELETE recursively %ws (err=%d)\n", ctx->url, err);
			switch (err)
			{
			case ERROR_SUCCESS:
				ctx->sendBuf->buf = HTTP_ERR_RESPONCE::delete_ok;
				ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::delete_ok);
				break;
			case ERROR_FILE_NOT_FOUND:
				ctx->sendBuf->buf = HTTP_ERR_RESPONCE::delete_not_found;
				ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::delete_not_found);
				break;
			default:
				ctx->sendBuf->buf = HTTP_ERR_RESPONCE::delete_err;
				ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::delete_err);
				break;
			}
			ctx->state = State::AfterSendHTML;
			WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
		}
		else {
			HANDLE hFile = CreateFileW(ctx->url,
				DELETE,
				0,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_DIRECTORY | FILE_FLAG_BACKUP_SEMANTICS,
				NULL);
			BOOL ok;
			if (hFile != INVALID_HANDLE_VALUE) {
				FILE_DISPOSITION_INFO d = { TRUE };
				ok = SetFileInformationByHandle(hFile, FileDispositionInfo, &d, sizeof d);
				CloseHandle(hFile);
			}
			else {
				ok = FALSE;
			}
			log_fmt("[info] DELETE %ws (%s)\n", ctx->url, ok ? "ok" : "failed");
			if (ok) {
				ctx->sendBuf->buf = HTTP_ERR_RESPONCE::delete_ok;
				ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::delete_ok);
			}
			else {
				log_fmt("[error] delte file, err=%d\n", GetLastError());
				switch (GetLastError())
				{
				case ERROR_DIR_NOT_EMPTY:
					ctx->sendBuf->buf = HTTP_ERR_RESPONCE::delete_access_denied;
					ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::delete_access_denied);
					break;
				case ERROR_FILE_NOT_FOUND:
					ctx->sendBuf->buf = HTTP_ERR_RESPONCE::delete_not_found;
					ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::delete_not_found);
					break;
				default:
					ctx->sendBuf->buf = HTTP_ERR_RESPONCE::delete_err;
					ctx->sendBuf->len = cstrlen(HTTP_ERR_RESPONCE::delete_err);
					break;
				}
			}
			ctx->state = State::AfterSendHTML;
			WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
		}
	}break;
	default:
	{
		errBuf = &HTTP_ERR_RESPONCE::method_not_allowed;
	BAD_REQUEST_AND_RELEASE:
		ctx->state = State::AfterSendHTML;
		WSASend(ctx->client, errBuf, 1, NULL, 0, &ctx->sendOL, NULL);
	}
	}
}