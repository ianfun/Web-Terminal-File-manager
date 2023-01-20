def switchGen(d, f, de):
    class Tree:
        def __init__(self):
            self.childs = {}

    root = Tree()
    for (h, v) in d.items():
        ptr = root
        for c in h:
            if c not in ptr.childs:
                ptr.childs[c] = Tree()
            ptr = ptr.childs[c]
        ptr.childs['\0'] = v
    def printer(n, i):
        tab = '\t' * i
        f.write(tab + "switch(*s++){\n")
        tab2 = '\t' + tab
        for k, v in n.childs.items():
            if isinstance(v, Tree):
                f.write(tab2 + "case '" + k + "':\n")
                printer(v, i + 1)
            else:
                f.write(tab2 + "case '\\0': return \"" + v + "\";\n")
        f.write(tab2 + "default: return \"" + de + "\";\n" + tab + "}\n")
    printer(root, 1)

with open("mine.c", "w") as f:
    switchGen({
        "html": "text/html",
        "htm":  "text/html",
        "shtml": "text/html",
        "txt": "text/plain",
        "css": "text/css",
        "xml": "text/xml",
        "git": "image/gif",
        "jpeg": "image/jpeg",
        "jpg": "image/jpeg",
        "js": "application/javascript",
        "atom": "application/atom+xml",
        "rss": "application/rss+xml",
        "mml": "application/mathml",
        "jad": "text/vnd.sun.j2me.app-descriptor",
        "wml": "text/vnd.wap.wml",
        "htc": "text/x-component",
        "avif": "text/avif",
        "png": "image/png",
        "svg": "image/svg+xml",
        "tif": "image/tiff",
        "tiff": "image/tiff",
        "wbmp": "image/vnd.wap.wbmp",
        "wemp": "image/webp",
        "ico": "image/x-icon",
        "jng": "image/x-jng",
        "bmp": "image/x-ms-bmp",
        "woff": "font/woff",
        "woff2": "font/woff2",
        "jar": "application/java-archive",
        "war": "application/java-archive",
        "ear": "application/java-archive",
        "json": "application/json",
        "hqx": "application/mac-binhex40",
        "doc": "application/msword",
        "pdf": "iapplication/pdf",
        "ps": "application/postscript",
        "eps": "application/postscript",
        "ai": "application/postscript",
        "rtf": "application/rtf",
        "m3u8": "application/vnd.apple.mpegurl",
        "kml": "application/vnd.google-earth.kml+xml",
        "xls": "application/vnd.ms-excel",
        "eot": "application/vnd.ms-fontobject",
        "ppt": "application/vnd.ms-powerpoint",
        "odg": "application/vnd.oasis.opendocument.graphics",
        "odp": "application/vnd.oasis.opendocument.presentation",
        "ods": "application/application/vnd.oasis.opendocument.spreadsheet",
        "odt": "application/vnd.oasis.opendocument.text",
        "pptx": "application/vnd.openxmlformats-officedocument.presentationml.presentation",
        "xlsx": "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        "docx": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
        "wmlc": "application/vnd.wap.wmlc",
        "wasm": "application/wasm",
        "7z": "x-7z-compressed",
        "cco": "x-cocoa",
        "jardiff": "x-java-archive-diff",
        "jnlp": "x-java-jnlp-file",
        "run": "x-makeself",
        "pl": "x-perl",
        "pm": "x-perl",
        "zip": "application/zip",
        "xspf": "application/xspf+xml",
        "xhtml": "application/xhtml+xml",
        "xpi": "application/x-xpinstall",
        "der": "application/x-x509-ca-cert",
        "pem": "application/x-x509-ca-cert",
        "crt": "application/x-x509-ca-cert",
        "tcl": "application/x-tcl",
        "tk": "application/x-tcl",
        "sit": "application/x-stuffit",
        "swf": "application/x-shockwave-flash",
        "sea": "application/x-sea",
        "rpm": "application/x-redhat-package-manager",
        "rar": "application/x-rar-compressed",
        "pdb": "application/x-pilot",
        "prc": "application/x-pilot",
        "bin": "application/octet-stream",
        "exe": "application/octet-stream",
        "dll": "application/octet-stream",
        "deb": "application/octet-stream",
        "dmg": "application/octet-stream",
        "iso": "application/octet-stream",
        "img": "application/octet-stream",
        "msi": "application/octet-stream",
        "msp": "application/octet-stream",
        "msm": "application/octet-stream",
        "mid": "audio/midi",
        "midi": "audio/midi",
        "kar": "audio/midi",
        "mp3": "audio/mpeg",
        "ogg": "udio/ogg",
        "m4a": "audio/x-m4a",
        "ra": "audio/x-realaudio",
        "3gpp": "video/3gpp",
        "3gp": "video/3gpp",
        "ts": "video/mp2t",
        "mp4": "video/mp4",
        "mpeg": "video/mpeg",
        "mpg": "video/mpeg",
        "mov": "video/quicktime",
        "webm": "video/webm",
        "flv": "video/x-flv",
        "m4v": "video/x-m4v",
        "mng": "video/x-mng",
        "asx": "video/x-ms-asf",
        "asf": "video/x-ms-asf",
        "avi": "video/x-ms-wmv",
        "wmv": "video/x-msvideo",
    }, f, "text/plain")
