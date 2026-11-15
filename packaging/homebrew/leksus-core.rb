class LeksusCore < Formula
  desc "Experimental Leksika native bootstrap core (not the full runtime)"
  homepage "https://github.com/zmlsaggg/Leksika"
  version "0.0.0"
  license "MIT"

  on_macos do
    on_arm do
      url "https://github.com/zmlsaggg/Leksika/releases/download/v#{version}/leksika-native-macos-arm64.tar.gz"
      sha256 "0000000000000000000000000000000000000000000000000000000000000000"
    end
    on_intel do
      url "https://github.com/zmlsaggg/Leksika/releases/download/v#{version}/leksika-native-macos-x64.tar.gz"
      sha256 "0000000000000000000000000000000000000000000000000000000000000000"
    end
  end
  on_linux do
    on_intel do
      url "https://github.com/zmlsaggg/Leksika/releases/download/v#{version}/leksika-native-linux-x64.tar.gz"
      sha256 "0000000000000000000000000000000000000000000000000000000000000000"
    end
  end

  def install
    bin.install "leksus-core"
    pkgshare.install "dictionary.lxd", "example.lek", "README.txt", "LICENSE.txt", "DICTIONARY-LICENSE.txt"
  end

  def caveats
    "For dictionary commands, pass #{pkgshare}/dictionary.lxd explicitly. This is not the full Leksika runtime."
  end

  test do
    system bin/"leksus-core", "--self-test"
  end
end
