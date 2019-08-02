# rsync

## 依赖

- OpenSSL
如果cmake提示找不到openssl请提供 -DOPENSSL_ROOT_DIR=/path/to/openssl

## 可执行文件

- rsync [file1] [file2] - 根据 file1 file2 生成 output
- jsonresult [jsonFile] [file2] - jsonFile 为分块信息json文件，file2 为新文件，生成增量信息文件 result.msg
