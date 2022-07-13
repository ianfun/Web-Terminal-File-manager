DWORD __stdcall readLoop(LPVOID ctx);

VOID NTAPI onProcessExit(PVOID context, BOOLEAN timeout) {
	assert(timeout == 0);
	IOCP* ctx = reinterpret_cast<IOCP*>(context);
	ctx->waitHandle = NULL;
	if (ctx->hReadThread) {
		if (CancelSynchronousIo(ctx->hReadThread) == FALSE) {
			assert(0);
		}
		CloseHandle(ctx->hReadThread);
		ctx->hReadThread = NULL;
	}
	if (ctx->hProcess) {
		DWORD dwExitCode;
		const char* msg;
		if (GetExitCodeProcess(ctx->hProcess, &dwExitCode)) {
			msg = "process exit with code %u";
		}
		else {
			msg = "process was exited(failed to get exit code)";
		}
		int n = snprintf(ctx->buf + 2, sizeof(ctx->buf) - 2, msg, dwExitCode);
		websocketWrite(ctx, ctx->buf, n + 2, &ctx->sendOL, ctx->sendBuf, Websocket::Opcode::Close);
		if (CloseHandle(ctx->hProcess) == FALSE) {
			assert(0);
		}ctx->hProcess = NULL;
	}
	if (ctx->hPC) {
		ClosePseudoConsole(ctx->hPC);
		ctx->hPC = NULL;
	}
	if (ctx->hStdIn) {
		if (CloseHandle(ctx->hStdIn) == FALSE) {
			assert(0);
		}
		ctx->hStdIn = NULL;
	}
	if (ctx->hStdOut) {
		if (CloseHandle(ctx->hStdOut) == FALSE) {
			assert(0);
		}
		ctx->hStdOut = NULL;
	}
	if (ctx->addrlist) {
		DeleteProcThreadAttributeList(ctx->addrlist);
		free(ctx->addrlist);
		ctx->addrlist = NULL;
	}
}

BOOL spawn(WCHAR* cmd, IOCP* ctx) {
	HANDLE hPipePTYIn = INVALID_HANDLE_VALUE;
	HANDLE hPipePTYOut = INVALID_HANDLE_VALUE;
	if (CreatePipe(&hPipePTYIn, &ctx->hStdIn, NULL, 0) &&
		CreatePipe(&ctx->hStdOut, &hPipePTYOut, NULL, 0))
	{
		{
			if (CreatePseudoConsole(ctx->coord, hPipePTYIn, hPipePTYOut, 0, &ctx->hPC) != S_OK) {
				return FALSE;
			}
		}
		CloseHandle(hPipePTYOut);
		CloseHandle(hPipePTYIn);
		{
			STARTUPINFOEX st{};
			size_t attrListSize{};
			st.StartupInfo.cb = sizeof(STARTUPINFOEX);
			InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);
			st.lpAttributeList =
				reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attrListSize));
			if (st.lpAttributeList)
			{
				ctx->addrlist = st.lpAttributeList;
				if (InitializeProcThreadAttributeList(st.lpAttributeList, 1, 0, &attrListSize)) {
					if (UpdateProcThreadAttribute(st.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, ctx->hPC, sizeof(HPCON), NULL, NULL)) {
						PROCESS_INFORMATION proc{};
						if (CreateProcessW(
							NULL,
							cmd,
							NULL,
							NULL,
							FALSE,
							EXTENDED_STARTUPINFO_PRESENT,
							NULL,
							NULL,
							&st.StartupInfo,
							&proc)) {
							ctx->hReadThread = CreateThread(NULL, 0, readLoop, ctx, 0, 0);
							if (ctx->hReadThread != NULL) {
								if (RegisterWaitForSingleObject(
									&ctx->waitHandle,
									proc.hProcess,
									onProcessExit,
									ctx,
									INFINITE,
									WT_EXECUTEONLYONCE | WT_EXECUTEINIOTHREAD
								) == FALSE) {
									assert(0);
									CloseHandle(proc.hProcess);
								}
								CloseHandle(proc.hThread);
								ctx->hProcess = proc.hProcess;
								return TRUE;
							}
						}
					}
				}
				free((void*)st.lpAttributeList);
			}
		}
	}
	log_fmt("Error: spwan() failed with error: %d\n", GetLastError());
	return FALSE;
}

DWORD __stdcall readLoop(LPVOID p) {
	constexpr int bufferSize = 1024;
	IOCP* ctx = reinterpret_cast<IOCP*>(p);
	void* buffer = HeapAlloc(heap, 0, bufferSize);
	if (buffer == NULL) {
		return 1;
	}
	DWORD readed = 0;
	for (;;) {
		if (ReadFile(ctx->hStdOut, buffer, bufferSize, &readed, 0) == FALSE) {
#ifdef  _DEBUG
			if (GetLastError() != ERROR_OPERATION_ABORTED) {
				assert(0);
			}
#endif //  DEBUG
			break;
		}
		websocketWrite(ctx, (const char*)buffer, readed, &dumyOL, ctx->conpty);
	}
	HeapFree(heap, 0, buffer);
	return 0;
}