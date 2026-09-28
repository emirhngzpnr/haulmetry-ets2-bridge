# Haulmetry ETS2 Bridge

## Calistirma

Visual Studio 2022 ile bu klasoru acin. `x64-debug` secin ve CMake yapilandirmasinin bitmesini bekleyin. Eski hata gorunuyorsa **Delete Cache and Reconfigure** kullanin. Baslangic hedefi olarak `haulmetry-ets2-bridge.exe` secip Ctrl+F5 ile calistirin.

PowerShell alternatifi:

```powershell
.\build.ps1
# Release icin:
.\build.ps1 -Configuration Release
```

Beklenen cikti: `libcurl initialized successfully.`

Mevcut kod yalnizca libcurl baslatma testidir. Henuz JSON gondermez veya ETS2 telemetrisi okumaz.

## Yerel bagimlilik kurulumu

vcpkg 2025.06.13 surumune sabitlenmistir. Araclar ve kutuphaneler `C:/Users/Public/haulmetry-ets2-bridge-tools` altindadir. MSVC/Ninja baglayici yanit dosyalari Turkce kullanici adini yanlis okudugu icin bagimliliklar ASCII bir yolda tutulur. Kaynak kod bu masaustu klasorunde kalir.

CMakePresets.json bu bilgisayara ait arac ve kurulum yollarini icerir. Baska bilgisayarda bu yollari kendi ASCII vcpkg yolunuzla degistirin; vcpkg-configuration.json icindeki baseline surumunu kullanin. Proje icindeki `.tools` ilk denemelerin onbellegidir, etkin kurulum Public klasorundedir.

Ilk degisikliklerden onceki CMake dosyalari `work/backup-20260927-201637` altinda yedeklendi. Derleme kayitlari `work/configure.log` dosyasindadir.
