if (-not ($env:GITHUB_ENV))
{
    if (Test-Path .env)
    {
        Get-Content .env | ForEach-Object {
            $name, $value = $_.split('=')
            Write-Output "$name=$value"
            Set-Content env:\$name $value
        }
    }
    xmake repo --update            # https://github.com/libxse/commonlibsse-ng-template?tab=readme-ov-file#upgrading-packages-optional
    xmake require --upgrade        # https://github.com/libxse/commonlibsse-ng-template?tab=readme-ov-file#upgrading-packages-optional
    xmake project -k cmakelists    # for clion, reference: https://github.com/libxse/commonlibsse-ng-template?tab=readme-ov-file#project-generation-optional
}
xmake -y OBody