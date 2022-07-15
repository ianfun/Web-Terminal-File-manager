#pragma once

typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
#define NT_SUCCESS(Status)  (((NTSTATUS)(Status)) >= 0)

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
#ifdef MIDL_PASS
	[size_is(MaximumLength / 2), length_is((Length) / 2)] USHORT* Buffer;
#else // MIDL_PASS
	_Field_size_bytes_part_(MaximumLength, Length) PWCH   Buffer;
#endif // MIDL_PASS
} UNICODE_STRING;
typedef UNICODE_STRING* PUNICODE_STRING;
typedef const UNICODE_STRING* PCUNICODE_STRING;
typedef enum _IO_PRIORITY_HINT {
    IoPriorityVeryLow,
    IoPriorityLow,
    IoPriorityNormal,
    IoPriorityHigh,
    IoPriorityCritical,
    MaxIoPriorityTypes
} IO_PRIORITY_HINT;
typedef enum _FILE_KNOWN_FOLDER_TYPE {
    KnownFolderNone,
    KnownFolderDesktop,
    KnownFolderDocuments,
    KnownFolderDownloads,
    KnownFolderMusic,
    KnownFolderPictures,
    KnownFolderVideos,
    KnownFolderOther,
    KnownFolderMax
} FILE_KNOWN_FOLDER_TYPE;

typedef enum _FILE_INFORMATION_CLASS {
    FileDirectoryInformation = 1,
    FileFullDirectoryInformation,                   // 2
    FileBothDirectoryInformation,                   // 3
    FileBasicInformation,                           // 4
    FileStandardInformation,                        // 5
    FileInternalInformation,                        // 6
    FileEaInformation,                              // 7
    FileAccessInformation,                          // 8
    FileNameInformation,                            // 9
    FileRenameInformation,                          // 10
    FileLinkInformation,                            // 11
    FileNamesInformation,                           // 12
    FileDispositionInformation,                     // 13
    FilePositionInformation,                        // 14
    FileFullEaInformation,                          // 15
    FileModeInformation,                            // 16
    FileAlignmentInformation,                       // 17
    FileAllInformation,                             // 18
    FileAllocationInformation,                      // 19
    FileEndOfFileInformation,                       // 20
    FileAlternateNameInformation,                   // 21
    FileStreamInformation,                          // 22
    FilePipeInformation,                            // 23
    FilePipeLocalInformation,                       // 24
    FilePipeRemoteInformation,                      // 25
    FileMailslotQueryInformation,                   // 26
    FileMailslotSetInformation,                     // 27
    FileCompressionInformation,                     // 28
    FileObjectIdInformation,                        // 29
    FileCompletionInformation,                      // 30
    FileMoveClusterInformation,                     // 31
    FileQuotaInformation,                           // 32
    FileReparsePointInformation,                    // 33
    FileNetworkOpenInformation,                     // 34
    FileAttributeTagInformation,                    // 35
    FileTrackingInformation,                        // 36
    FileIdBothDirectoryInformation,                 // 37
    FileIdFullDirectoryInformation,                 // 38
    FileValidDataLengthInformation,                 // 39
    FileShortNameInformation,                       // 40
    FileIoCompletionNotificationInformation,        // 41
    FileIoStatusBlockRangeInformation,              // 42
    FileIoPriorityHintInformation,                  // 43
    FileSfioReserveInformation,                     // 44
    FileSfioVolumeInformation,                      // 45
    FileHardLinkInformation,                        // 46
    FileProcessIdsUsingFileInformation,             // 47
    FileNormalizedNameInformation,                  // 48
    FileNetworkPhysicalNameInformation,             // 49
    FileIdGlobalTxDirectoryInformation,             // 50
    FileIsRemoteDeviceInformation,                  // 51
    FileUnusedInformation,                          // 52
    FileNumaNodeInformation,                        // 53
    FileStandardLinkInformation,                    // 54
    FileRemoteProtocolInformation,                  // 55

    //
    //  These are special versions of these operations (defined earlier)
    //  which can be used by kernel mode drivers only to bypass security
    //  access checks for Rename and HardLink operations.  These operations
    //  are only recognized by the IOManager, a file system should never
    //  receive these.
    //

    FileRenameInformationBypassAccessCheck,         // 56
    FileLinkInformationBypassAccessCheck,           // 57

    //
    // End of special information classes reserved for IOManager.
    //

    FileVolumeNameInformation,                      // 58
    FileIdInformation,                              // 59
    FileIdExtdDirectoryInformation,                 // 60
    FileReplaceCompletionInformation,               // 61
    FileHardLinkFullIdInformation,                  // 62
    FileIdExtdBothDirectoryInformation,             // 63
    FileDispositionInformationEx,                   // 64
    FileRenameInformationEx,                        // 65
    FileRenameInformationExBypassAccessCheck,       // 66
    FileDesiredStorageClassInformation,             // 67
    FileStatInformation,                            // 68
    FileMemoryPartitionInformation,                 // 69
    FileStatLxInformation,                          // 70
    FileCaseSensitiveInformation,                   // 71
    FileLinkInformationEx,                          // 72
    FileLinkInformationExBypassAccessCheck,         // 73
    FileStorageReserveIdInformation,                // 74
    FileCaseSensitiveInformationForceAccessCheck,   // 75
    FileKnownFolderInformation,   // 76

    FileMaximumInformation
} FILE_INFORMATION_CLASS, * PFILE_INFORMATION_CLASS;
typedef struct _OBJECT_ATTRIBUTES {
    ULONG           Length;
    HANDLE          RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG           Attributes;
    PVOID           SecurityDescriptor;
    PVOID           SecurityQualityOfService;
} OBJECT_ATTRIBUTES, * POBJECT_ATTRIBUTES;

typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    } u;
    ULONG_PTR Information;
} IO_STATUS_BLOCK, * PIO_STATUS_BLOCK;
typedef VOID
(NTAPI* PIO_APC_ROUTINE)(
    IN PVOID ApcContext,
    IN PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG Reserved);

__kernel_entry NTSTATUS NtOpenFile(
    _Out_ PHANDLE            FileHandle,
    _In_  ACCESS_MASK        DesiredAccess,
     _In_ POBJECT_ATTRIBUTES ObjectAttributes,
    _Out_ PIO_STATUS_BLOCK   IoStatusBlock,
    _In_  ULONG              ShareAccess,
    _In_ ULONG              OpenOptions
);
__kernel_entry NTSTATUS NtClose(
    _In_ HANDLE Handle
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtQueryDirectoryFile(
    _In_ HANDLE FileHandle,
    _In_opt_ HANDLE Event,
    _In_opt_ PIO_APC_ROUTINE ApcRoutine,
    _In_opt_ PVOID ApcContext,
    _Out_ PIO_STATUS_BLOCK IoStatusBlock,
    _Out_writes_bytes_(Length) PVOID FileInformation,
    _In_ ULONG Length,
    _In_ FILE_INFORMATION_CLASS FileInformationClass,
    _In_ BOOLEAN ReturnSingleEntry,
    _In_opt_ PUNICODE_STRING FileName,
    _In_ BOOLEAN RestartScan
);

#define STATUS_NO_MORE_FILES             ((NTSTATUS)0x80000006L)

typedef struct _FILE_EA_INFORMATION {
    ULONG EaSize;
} FILE_EA_INFORMATION, * PFILE_EA_INFORMATION;

typedef struct _FILE_POSITION_INFORMATION {
    LARGE_INTEGER CurrentByteOffset;
} FILE_POSITION_INFORMATION, * PFILE_POSITION_INFORMATION;

typedef struct _FILE_DIRECTORY_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    WCHAR         FileName[1];
} FILE_DIRECTORY_INFORMATION, * PFILE_DIRECTORY_INFORMATION;

typedef struct _FILE_FULL_DIR_INFORMATION {
	ULONG NextEntryOffset;
	ULONG FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	ULONG FileAttributes;
	ULONG FileNameLength;
	ULONG EaSize;
	WCHAR FileName[1];
} FILE_FULL_DIR_INFORMATION, * PFILE_FULL_DIR_INFORMATION;

typedef struct _FILE_BOTH_DIR_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    ULONG         EaSize;
    CCHAR         ShortNameLength;
    WCHAR         ShortName[12];
    WCHAR         FileName[1];
} FILE_BOTH_DIR_INFORMATION, * PFILE_BOTH_DIR_INFORMATION;

typedef struct _FILE_BASIC_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    ULONG         FileAttributes;
} FILE_BASIC_INFORMATION, * PFILE_BASIC_INFORMATION;

typedef struct _FILE_STANDARD_INFORMATION {
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG         NumberOfLinks;
    BOOLEAN       DeletePending;
    BOOLEAN       Directory;
} FILE_STANDARD_INFORMATION, * PFILE_STANDARD_INFORMATION;

typedef struct _FILE_INTERNAL_INFORMATION {
    LARGE_INTEGER IndexNumber;
} FILE_INTERNAL_INFORMATION, * PFILE_INTERNAL_INFORMATION;

typedef struct _FILE_KNOWN_FOLDER_INFORMATION {
    FILE_KNOWN_FOLDER_TYPE Type;
} FILE_KNOWN_FOLDER_INFORMATION, * PFILE_KNOWN_FOLDER_INFORMATION;

typedef struct _FILE_STORAGE_RESERVE_ID_INFORMATION {
    STORAGE_RESERVE_ID StorageReserveId;
} FILE_STORAGE_RESERVE_ID_INFORMATION, * PFILE_STORAGE_RESERVE_ID_INFORMATION;

typedef struct _FILE_CASE_SENSITIVE_INFORMATION {
    ULONG Flags;
} FILE_CASE_SENSITIVE_INFORMATION, * PFILE_CASE_SENSITIVE_INFORMATION;

typedef struct _FILE_STAT_LX_INFORMATION {
    LARGE_INTEGER FileId;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG         FileAttributes;
    ULONG         ReparseTag;
    ULONG         NumberOfLinks;
    ACCESS_MASK   EffectiveAccess;
    ULONG         LxFlags;
    ULONG         LxUid;
    ULONG         LxGid;
    ULONG         LxMode;
    ULONG         LxDeviceIdMajor;
    ULONG         LxDeviceIdMinor;
} FILE_STAT_LX_INFORMATION, * PFILE_STAT_LX_INFORMATION;

typedef struct _FILE_STAT_INFORMATION {
    LARGE_INTEGER FileId;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG         FileAttributes;
    ULONG         ReparseTag;
    ULONG         NumberOfLinks;
    ACCESS_MASK   EffectiveAccess;
} FILE_STAT_INFORMATION, * PFILE_STAT_INFORMATION;

typedef struct _FILE_DISPOSITION_INFORMATION_EX {
    ULONG Flags;
} FILE_DISPOSITION_INFORMATION_EX, * PFILE_DISPOSITION_INFORMATION_EX;

typedef struct _FILE_COMPLETION_INFORMATION {
    HANDLE Port;
    PVOID  Key;
} FILE_COMPLETION_INFORMATION, * PFILE_COMPLETION_INFORMATION;

typedef struct _FILE_REMOTE_PROTOCOL_INFORMATION {
    USHORT StructureVersion;
    USHORT StructureSize;
    ULONG  Protocol;
    USHORT ProtocolMajorVersion;
    USHORT ProtocolMinorVersion;
    USHORT ProtocolRevision;
    USHORT Reserved;
    ULONG  Flags;
    struct {
        ULONG Reserved[8];
    } GenericReserved;
    struct {
        ULONG Reserved[16];
    } ProtocolSpecificReserved;
    union {
        struct {
            struct {
                ULONG Capabilities;
            } Server;
            struct {
                ULONG Capabilities;
                ULONG ShareFlags;
                ULONG CachingFlags;
                UCHAR ShareType;
                UCHAR Reserved0[3];
                ULONG Reserved1;
            } Share;
        } Smb2;
        ULONG Reserved[16];
    } ProtocolSpecific;
} FILE_REMOTE_PROTOCOL_INFORMATION, * PFILE_REMOTE_PROTOCOL_INFORMATION;

typedef struct _FILE_ID_GLOBAL_TX_DIR_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    LARGE_INTEGER FileId;
    GUID          LockingTransactionId;
    ULONG         TxInfoFlags;
    WCHAR         FileName[1];
} FILE_ID_GLOBAL_TX_DIR_INFORMATION, * PFILE_ID_GLOBAL_TX_DIR_INFORMATION;

typedef struct _FILE_IO_PRIORITY_HINT_INFORMATION {
    IO_PRIORITY_HINT PriorityHint;
} FILE_IO_PRIORITY_HINT_INFORMATION, * PFILE_IO_PRIORITY_HINT_INFORMATION;

typedef struct _FILE_VALID_DATA_LENGTH_INFORMATION {
    LARGE_INTEGER ValidDataLength;
} FILE_VALID_DATA_LENGTH_INFORMATION, * PFILE_VALID_DATA_LENGTH_INFORMATION;

typedef struct _FILE_ID_FULL_DIR_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    ULONG         EaSize;
    LARGE_INTEGER FileId;
    WCHAR         FileName[1];
} FILE_ID_FULL_DIR_INFORMATION, * PFILE_ID_FULL_DIR_INFORMATION;

typedef struct _FILE_ID_BOTH_DIR_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG         FileAttributes;
    ULONG         FileNameLength;
    ULONG         EaSize;
    CCHAR         ShortNameLength;
    WCHAR         ShortName[12];
    LARGE_INTEGER FileId;
    WCHAR         FileName[1];
} FILE_ID_BOTH_DIR_INFORMATION, * PFILE_ID_BOTH_DIR_INFORMATION;

typedef struct _FILE_ATTRIBUTE_TAG_INFORMATION {
    ULONG FileAttributes;
    ULONG ReparseTag;
} FILE_ATTRIBUTE_TAG_INFORMATION, * PFILE_ATTRIBUTE_TAG_INFORMATION;
typedef struct _FILE_NAME_INFORMATION {
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_NAME_INFORMATION, * PFILE_NAME_INFORMATION;

typedef struct _FILE_LINK_ENTRY_INFORMATION {
    ULONG    NextEntryOffset;
    LONGLONG ParentFileId;
    ULONG    FileNameLength;
    WCHAR    FileName[1];
} FILE_LINK_ENTRY_INFORMATION, * PFILE_LINK_ENTRY_INFORMATION;

typedef struct _FILE_LINKS_INFORMATION {
    ULONG                       BytesNeeded;
    ULONG                       EntriesReturned;
    FILE_LINK_ENTRY_INFORMATION Entry;
} FILE_LINKS_INFORMATION, * PFILE_LINKS_INFORMATION;

typedef struct _FILE_NETWORK_PHYSICAL_NAME_INFORMATION {
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_NETWORK_PHYSICAL_NAME_INFORMATION, * PFILE_NETWORK_PHYSICAL_NAME_INFORMATION;

typedef struct _FILE_NETWORK_OPEN_INFORMATION {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG         FileAttributes;
} FILE_NETWORK_OPEN_INFORMATION, * PFILE_NETWORK_OPEN_INFORMATION;

typedef struct _FILE_REPARSE_POINT_INFORMATION {
    LONGLONG FileReference;
    ULONG    Tag;
} FILE_REPARSE_POINT_INFORMATION, * PFILE_REPARSE_POINT_INFORMATION;

typedef struct _FILE_QUOTA_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         SidLength;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER QuotaUsed;
    LARGE_INTEGER QuotaThreshold;
    LARGE_INTEGER QuotaLimit;
    SID           Sid;
} FILE_QUOTA_INFORMATION, * PFILE_QUOTA_INFORMATION;

typedef struct _FILE_PIPE_LOCAL_INFORMATION {
    ULONG NamedPipeType;
    ULONG NamedPipeConfiguration;
    ULONG MaximumInstances;
    ULONG CurrentInstances;
    ULONG InboundQuota;
    ULONG ReadDataAvailable;
    ULONG OutboundQuota;
    ULONG WriteQuotaAvailable;
    ULONG NamedPipeState;
    ULONG NamedPipeEnd;
} FILE_PIPE_LOCAL_INFORMATION, * PFILE_PIPE_LOCAL_INFORMATION;

typedef struct _FILE_PIPE_REMOTE_INFORMATION {
    LARGE_INTEGER CollectDataTime;
    ULONG         MaximumCollectionCount;
} FILE_PIPE_REMOTE_INFORMATION, * PFILE_PIPE_REMOTE_INFORMATION;

typedef struct _FILE_MAILSLOT_QUERY_INFORMATION {
    ULONG         MaximumMessageSize;
    ULONG         MailslotQuota;
    ULONG         NextMessageSize;
    ULONG         MessagesAvailable;
    LARGE_INTEGER ReadTimeout;
} FILE_MAILSLOT_QUERY_INFORMATION, * PFILE_MAILSLOT_QUERY_INFORMATION;

typedef struct _FILE_MAILSLOT_SET_INFORMATION {
    PLARGE_INTEGER ReadTimeout;
} FILE_MAILSLOT_SET_INFORMATION, * PFILE_MAILSLOT_SET_INFORMATION;

typedef struct _FILE_COMPRESSION_INFORMATION {
    LARGE_INTEGER CompressedFileSize;
    USHORT        CompressionFormat;
    UCHAR         CompressionUnitShift;
    UCHAR         ChunkShift;
    UCHAR         ClusterShift;
    UCHAR         Reserved[3];
} FILE_COMPRESSION_INFORMATION, * PFILE_COMPRESSION_INFORMATION;


typedef struct _FILE_OBJECTID_INFORMATION {
    LONGLONG FileReference;
    UCHAR    ObjectId[16];
    union {
        struct {
            UCHAR BirthVolumeId[16];
            UCHAR BirthObjectId[16];
            UCHAR DomainId[16];
        } DUMMYSTRUCTNAME;
        UCHAR ExtendedInfo[48];
    } DUMMYUNIONNAME;
} FILE_OBJECTID_INFORMATION, * PFILE_OBJECTID_INFORMATION;

typedef struct _FILE_STREAM_INFORMATION {
    ULONG         NextEntryOffset;
    ULONG         StreamNameLength;
    LARGE_INTEGER StreamSize;
    LARGE_INTEGER StreamAllocationSize;
    WCHAR         StreamName[1];
} FILE_STREAM_INFORMATION, * PFILE_STREAM_INFORMATION;

typedef struct _FILE_ACCESS_INFORMATION {
    ACCESS_MASK AccessFlags;
} FILE_ACCESS_INFORMATION, * PFILE_ACCESS_INFORMATION;

typedef struct _FILE_MODE_INFORMATION {
    ULONG Mode;
} FILE_MODE_INFORMATION, * PFILE_MODE_INFORMATION;

typedef struct _FILE_ALIGNMENT_INFORMATION {
    ULONG AlignmentRequirement;
} FILE_ALIGNMENT_INFORMATION, * PFILE_ALIGNMENT_INFORMATION;

typedef struct _FILE_ALL_INFORMATION {
    FILE_BASIC_INFORMATION     BasicInformation;
    FILE_STANDARD_INFORMATION  StandardInformation;
    FILE_INTERNAL_INFORMATION  InternalInformation;
    FILE_EA_INFORMATION        EaInformation;
    FILE_ACCESS_INFORMATION    AccessInformation;
    FILE_POSITION_INFORMATION  PositionInformation;
    FILE_MODE_INFORMATION      ModeInformation;
    FILE_ALIGNMENT_INFORMATION AlignmentInformation;
    FILE_NAME_INFORMATION      NameInformation;
} FILE_ALL_INFORMATION, * PFILE_ALL_INFORMATION;

typedef struct _FILE_FULL_EA_INFORMATION {
    ULONG  NextEntryOffset;
    UCHAR  Flags;
    UCHAR  EaNameLength;
    USHORT EaValueLength;
    CHAR   EaName[1];
} FILE_FULL_EA_INFORMATION, * PFILE_FULL_EA_INFORMATION;

typedef struct _FILE_DISPOSITION_INFORMATION {
    BOOLEAN DeleteFile;
} FILE_DISPOSITION_INFORMATION, * PFILE_DISPOSITION_INFORMATION;

typedef struct _FILE_NAMES_INFORMATION {
    ULONG NextEntryOffset;
    ULONG FileIndex;
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_NAMES_INFORMATION, * PFILE_NAMES_INFORMATION;

typedef struct _FILE_LINK_INFORMATION {
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN10_RS5)
    union {
        BOOLEAN ReplaceIfExists;  // FileLinkInformation
        ULONG Flags;              // FileLinkInformationEx
    } DUMMYUNIONNAME;
#else
    BOOLEAN ReplaceIfExists;
#endif
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_LINK_INFORMATION, * PFILE_LINK_INFORMATION;

typedef struct _FILE_RENAME_INFORMATION {
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN10_RS1)
    union {
        BOOLEAN ReplaceIfExists;  // FileRenameInformation
        ULONG Flags;              // FileRenameInformationEx
    } DUMMYUNIONNAME;
#else
    BOOLEAN ReplaceIfExists;
#endif
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_RENAME_INFORMATION, * PFILE_RENAME_INFORMATION;

typedef struct _FILE_ALLOCATION_INFORMATION {
    LARGE_INTEGER AllocationSize;
} FILE_ALLOCATION_INFORMATION, * PFILE_ALLOCATION_INFORMATION;


typedef struct _FILE_END_OF_FILE_INFORMATION {
    LARGE_INTEGER EndOfFile;
} FILE_END_OF_FILE_INFORMATION, * PFILE_END_OF_FILE_INFORMATION;

typedef struct _FILE_PIPE_INFORMATION {
    ULONG ReadMode;
    ULONG CompletionMode;
} FILE_PIPE_INFORMATION, * PFILE_PIPE_INFORMATION;

