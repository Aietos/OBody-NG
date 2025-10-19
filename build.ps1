if (-not ($env:GITHUB_ENV) -and (Test-Path .env))
{
    Get-Content .env | ForEach-Object {
        $name, $value = $_.split('=')
        Write-Output "$name=$value"
        Set-Content env:\$name $value
    }
    xmake repo --update                                 # Readme.md
    xmake require --upgrade                             # Readme.md
    xmake project -k cmakelists                         # for clion
}
xmake -y OBody