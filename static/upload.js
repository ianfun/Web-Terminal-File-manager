var upload;
var names = [];
var menu;
var uploadMenu;
var newFolderMenu;
var deleteTarget;
var table;
var d;
var attrs;
const FILE_ATTRIBUTE_READONLY = 0x00000001;
const FILE_ATTRIBUTE_HIDDEN = 0x00000002;
const FILE_ATTRIBUTE_SYSTEM = 0x00000004;
const FILE_ATTRIBUTE_DIRECTORY = 0x00000010;
const FILE_ATTRIBUTE_ARCHIVE = 0x00000020;
const FILE_ATTRIBUTE_DEVICE = 0x00000040;
const FILE_ATTRIBUTE_NORMAL = 0x00000080;
const FILE_ATTRIBUTE_TEMPORARY = 0x00000100;
const FILE_ATTRIBUTE_COMPRESSED = 0x00000800;
function goodURL() {
    var url = String(location.pathname);
    if (location.pathname.lastIndexOf(':') == -1) {
        while (url.endsWith("/")) {
            url = url.slice(0, -1);
        }
    }
    while (1) {
        var tmp = url.replaceAll("//", "/");
        if (url.length != tmp.length) {
            url = tmp;
            continue;
        }
        break;
    }
    history.pushState({}, "", tmp);
}
function getFileSize(s) {
    if (s == -1) {
        return "";
    }
    if (s < 1024) {
        return s.toString() + "";
    }
    if (s < 1024 * 1024) {
        return Math.ceil(s / 1024).toString() + " KB";
    }
    if (s < 1024 * 1024 * 1024) {
        return Math.ceil(s / (1024 * 1024)).toString() + " MB";
    }
    return Math.ceil(s / (1024 * 1024 * 1024)).toString() + " GB";
}
function newEntry(name, size, date) {
    var tname = document.createElement('td');
    var tsize = document.createElement('td');
    var tlast = document.createElement('td');

    var a = document.createElement('a');
    a.innerText = name;
    a.href = './' + name;
    tname.appendChild(a);
    tlast.innerText = date.toLocaleString();
    if (size != -1) {
        tsize.innerText = getFileSize(parseInt(size));
    }
    names.push(name);
    var tr = document.createElement('tr');
    tr.appendChild(tname);
    tr.appendChild(tsize);
    tr.appendChild(tlast);
    table.appendChild(tr);
    return a;
}
function showAttr(a, attr) {
    if (attr & FILE_ATTRIBUTE_ARCHIVE)
        a.classList.add("archive");
    if (attr & FILE_ATTRIBUTE_COMPRESSED)
        a.classList.add("compressed");
    if (attr & FILE_ATTRIBUTE_DEVICE)
        a.classList.add("device");
    if (attr & FILE_ATTRIBUTE_DIRECTORY)
        a.classList.add("directory");
    if (attr & FILE_ATTRIBUTE_HIDDEN)
        a.classList.add("hidden");
    if (attr & FILE_ATTRIBUTE_NORMAL)
        a.classList.add("normal");
    if (attr & FILE_ATTRIBUTE_READONLY)
        a.classList.add("readonly");
    if (attr & FILE_ATTRIBUTE_SYSTEM)
        a.classList.add("system");
    if (attr & FILE_ATTRIBUTE_TEMPORARY)
        a.classList.add("temporary");
}
window.onload = () => {
    d = document.querySelector("d");
    attrs = new Array(d.childNodes.length);
    table = document.createElement('table');
    {
        var tname = document.createElement('th');
        tname.innerText = "Name";
        var tsize = document.createElement('th');
        tsize.innerText = "Size";
        var tlast = document.createElement('th');
        tlast.innerText = "Last modified";
        var theader = document.createElement('tr');
        theader.appendChild(tname);
        theader.appendChild(tsize);
        theader.appendChild(tlast);
        table.appendChild(theader);
    }
    var i = 0;
    for (var c of d.childNodes) {
        var tr = document.createElement('tr');
        var tname = document.createElement('td');
        var tsize = document.createElement('td');
        var tlast = document.createElement('td');
        tr.appendChild(tname);
        tr.appendChild(tsize);
        tr.appendChild(tlast);
        // https://docs.microsoft.com/en-us/windows/win32/sysinfo/file-times
        var [size, time, attr] = c.getAttribute('z').split(";");
        attr = parseInt(attr);
        {
            var t = new Date(parseFloat(time) / 10000); // nanosecond to millisecond
            t.setUTCFullYear(t.getUTCFullYear() - 369); // 1970 - 1601
            tlast.innerText = t.toLocaleString();
        }
        var a = document.createElement('a');
        a.innerText = c.innerText;
        a.href = './' + c.innerText;
        tname.appendChild(a);
        names.push(c.innerText);
        tsize.innerText = getFileSize(attr & FILE_ATTRIBUTE_DIRECTORY ? -1:parseInt(size));
        table.appendChild(tr);
        showAttr(a, attrs[i] = attr);
        if (i & 1) {
            tr.style.backgroundColor = '#d4ee9085';
        } else {
            tr.style.backgroundColor = 'lightblue';
        }
        i++;
    }
    d.parentNode.removeChild(d);
    document.documentElement.setAttribute('lang', navigator.language.match(/\w+/)[0]);
    document.documentElement.appendChild(table);
    f = document.getElementById("f");
    upload = document.getElementById("upload");
    uploadMenu = document.createElement('button');
    newFolderMenu = document.createElement('button');
    menu = document.createElement('div');
    menu.style.backgroundColor = 'brown';
    menu.style.position = 'fixed';
    menu.style.borderRadius = '5px';
    menu.style.border = 'solid 5px brown';
    menu.appendChild(uploadMenu);
    menu.appendChild(newFolderMenu);
    menu.style.visibility = 'hidden';
    document.documentElement.appendChild(menu);
    normalMode();
    goodURL();
    let dir = decodeURIComponent(location.pathname.slice(1));
    document.querySelector('h1').innerText = "Directory listing for " + dir + " (" + d.getAttribute("d") + "'s computer)";
    var termina_here = document.createElement("button");
    termina_here.innerText = "Terminal here";
    termina_here.onclick = function () {
        sessionStorage.setItem("shell-dir", dir);
        var term = open("/index.html");
    }
    document.documentElement.appendChild(termina_here);
}
function normalMode() {
    uploadMenu.onclick = () => f.click();
    uploadMenu.innerText = 'Upload file';
    uploadMenu.style.backgroundColor = '';
    newFolderMenu.onclick = fnewFolder;
    newFolderMenu.innerText = 'Create folder';
    newFolderMenu.style.backgroundColor = '';
}
function deleteMode() {
    uploadMenu.onclick = fDelete;
    uploadMenu.innerText = 'Delete';
    uploadMenu.style.backgroundColor = 'red';
    newFolderMenu.onclick = fRename;
    newFolderMenu.innerText = "Rename";
    newFolderMenu.style.backgroundColor = 'yellow';
}
function fnewFolder() {
    var folder = prompt("Folder name");
    if (folder) {
        if (names.lastIndexOf(folder) != -1) {
            alert("The Folder alerdy exisits!");
        } else {
            var x = new XMLHttpRequest();
            x.onreadystatechange = () => {
                if (x.readyState === XMLHttpRequest.DONE) {
                    switch (x.status) {
                        case 201:
                            showAttr(newEntry(x.xfile, -1, x.date), FILE_ATTRIBUTE_DIRECTORY);
                            break;
                        case 404:
                            alert("The path to create is not in file system.This may due to intermediate path is not correct" + x.xfile);
                            break;
                        case 500:
                            alert("Error during creating directory.This may due to you has no permission for that folder." + x.xfile);
                            break;
                    }
                }
            };
            x.onerror = () => {
                alert("Network error during creating folder: " + x.folder);
            };
            x.xfile = folder;
            x.date = new Date();
            x.open("PUT", './' + folder);
            x.send(null);
        }
    }
}
function fDeleteRecursively(target) {
    var x = new XMLHttpRequest();
    x.target = target;
    x.open("DELETE", './' + deleteTarget.innerText);
    x.setRequestHeader("X-Recursively", "true");
    x.onerror = () => {
        alert("Network error during delete file: " + x.target.innerText);
    }
    x.onreadystatechange = () => {
        if (x.readyState === XMLHttpRequest.DONE) {
            switch (x.status) {
                case 200:
                    table.removeChild(x.target.parentNode.parentNode);
                    break;
                case 404:
                    alert(x.response || "The file to delete is not found.");
                    break;
                default:
                    console.error("invalid status code", x.status);
            }
        }
    };
    x.send(null);
}
function fDelete() {
    var x = new XMLHttpRequest();
    x.target = deleteTarget; // a
    x.open("DELETE", './' + deleteTarget.innerText);
    x.onerror = () => {
        alert("Network error during delete file: " + x.target.innerText);
    }
    x.onreadystatechange = () => {
        if (x.readyState === XMLHttpRequest.DONE) {
            switch (x.status) {
                case 200:
                    table.removeChild(x.target.parentNode.parentNode);
                    break;
                case 202:
                    if (confirm(x.response || "The directory is not not empty.Are you sure to remove recursively?")) {
                        fDeleteRecursively(x.target);
                    }
                    break;
                case 404:
                    alert(x.response || "The file to delete is not found.");
                    break;
                case 500:
                    alert("Delete " + x.target.innerText + " failed.Some un-defined error happened.");
                    break;
                default:
                    console.error("invalid status code", x.status);
            }
        }
    };
    x.send(null);
}
function fRename() {
    var oldname = deleteTarget.innerText;
    var newname = prompt("new name for " + oldname);
    if (newname) {
        if (names.lastIndexOf(newname) != -1) {
            if (confirm("the new name " + newname + " is already exists, replace?") == false) {
                return;
            }
        }
        var x = new XMLHttpRequest();
        x.oldname = oldname;
        x.newname = newname;
        x.target = deleteTarget;
        x.onerror = () => {
            alert("Network error during rename " + x.oldname + " to " + x.newname);
        }
        x.onreadystatechange = () => {
            if (x.readyState === XMLHttpRequest.DONE) {
                switch (x.status) {
                    case 200:
                        x.target.innerText = x.newname;
                        x.target.href = './' + x.newname;
                        break;
                    case 404:
                        alert(x.response || "The name(" + x.oldname + ") to rename is not found.");
                        break;
                    case 500:
                        alert("Error when rename " + x.oldname + " to " + x.newname);
                        break;
                    default:
                        console.error("invalid status code", x.status);
                }
            }
        };
        x.open("DELETE", './' + x.oldname);
        x.setRequestHeader("X-NewName", encodeURIComponent(location.pathname.slice(1) + '/') + encodeURIComponent(x.newname));
        x.send(null);
    }
}
function fsubmit(e) {
    if (f.files.length == 0) return false;
    for (var i = 0; i < f.files.length; ++i) {
        var overwrite = false;
        if (names.lastIndexOf(f.files[i].name) != -1) {
            if (false) {//confirm("over write " + f.files[i].name + "?") == false
                continue;
            }
            overwrite = true;
        }
        let x = new XMLHttpRequest();
        x.overwrite = overwrite;
        let progress = document.createElement("div");
        let outer = document.createElement('div');
        outer.classList.add('outer');
        progress.classList.add("upload");
        outer.appendChild(progress);
        upload.appendChild(outer);
        x.upload.onprogress = (e) => {
            if (e.lengthComputable) 
                progress.style.width = ~~((e.loaded / e.total) * 150) + 'px';
        };
        x.onloadend = (e) => {
            upload.removeChild(outer);
        };
        x.onerror = () => {
            alert("Sorry, the file " + x.xfile + ' upload failed\nmaybe try again?');
        };
        x.onreadystatechange = () => {
            if (x.readyState === XMLHttpRequest.DONE && x.status === 204) {
                if (x.overwrite) {
                    for (var i = 1; i < table.childNodes.length; ++i) {
                        let tr = table.childNodes[i];
                        if (tr.firstChild.firstChild.innerText == x.xfile.name) {
                            let tsize = tr.firstChild.nextSibling;
                            let tlast = tsize.nextSibling;
                            tsize.innerText = getFileSize(x.xfile.size);
                            tlast.innerText = x.xfile.lastModifiedDate.toLocaleString();
                            break;
                        }
                    }
                } else {
                    showAttr(newEntry(x.xfile.name, x.xfile.size, x.xfile.lastModifiedDate), FILE_ATTRIBUTE_NORMAL);
                }
            }
        };
        x.open("POST", './' + f.files[i].name);
        x.send(new Blob([f.files[i]]));
        x.xfile = f.files[i];
    }
    return false;
};

document.addEventListener('contextmenu', function (event) {
    event.preventDefault();
    menu.style.top = event.y.toString() + 'px';
    menu.style.left = event.x.toString() + 'px';
    menu.style.visibility = 'visible';
    if (event.target.href) {
        deleteMode();
        deleteTarget = event.target;
    } else {
        normalMode();
    }
})
document.addEventListener('click', function (event) {
    menu.style.visibility = 'hidden';
})
