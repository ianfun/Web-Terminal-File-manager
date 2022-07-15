#include <bcrypt.h>
#pragma comment (lib, "Crypt32.lib")
#pragma comment (lib, "bcrypt.lib")

static BCRYPT_ALG_HANDLE hAlgorithm=NULL;
static PBYTE hashO = NULL;
static BCRYPT_HASH_HANDLE  hHash = NULL;

BOOL initHash(){
    DWORD hashLen, dummy;
    // we need hash many times
    if ((BCryptOpenAlgorithmProvider( &hAlgorithm, BCRYPT_SHA1_ALGORITHM, NULL, BCRYPT_HASH_REUSABLE_FLAG)) < 0)
        return FALSE;

    if ((BCryptGetProperty(hAlgorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &dummy, 0)<0))    
        return FALSE;

    hashO = (PBYTE)HeapAlloc(heap, 0, hashLen);
    if (NULL == hashO)
    {
        return FALSE;
    }
    if ((BCryptCreateHash(
        hAlgorithm,
        &hHash,
        hashO,
        hashLen,
        NULL,
        0,
        0))<0)
    {
        return FALSE;
    }
    return TRUE;
}
void closeHash(){
    if (hAlgorithm)
    {
        BCryptCloseAlgorithmProvider(hAlgorithm, 0);
    }
    if (hHash)
    {
        BCryptDestroyHash(hHash);
    }
    if (hashO)
    {
        HeapFree(heap, 0, hashO);
    }
}
BOOL HashHanshake(const char* key, ULONG length, char *buf/*29 bytes*/){
    BYTE   sha1_o[20]{};
    // SHA-1 20 bytes hash
    if ((BCryptHashData(hHash, (PUCHAR)key, length, 0)) < 0)
        return FALSE;
    if ((BCryptFinishHash(hHash, sha1_o, sizeof(sha1_o), 0)) < 0)
        return FALSE;
    DWORD size=29;
    // base64 encoding
    return CryptBinaryToStringA(sha1_o, sizeof(sha1_o), CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF , buf, &size);
}
