echo "`n`nSetting --assume-unchanged on all cmakelists.txt files"
echo "cmakelists.txt files will NOT be pushed/synced by git`n`n"

$fileList = Get-ChildItem -Include cmakelists.txt -Recurse  -Name -Depth 2
foreach ($file in $fileList) 
{
    echo $file
    git update-index --no-assume-unchanged $file
}