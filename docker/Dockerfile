FROM ubuntu

RUN apt-get update

RUN apt-get install -y iputils-ping

# ZSH and Git
RUN apt-get -y install zsh
RUN apt-get -y install git
RUN apt-get -y install curl
RUN sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"

# Neovim
RUN curl -LO https://github.com/neovim/neovim/releases/latest/download/nvim-linux64.tar.gz
RUN rm -rf /opt/nvim
RUN tar -C /opt -xzf nvim-linux64.tar.gz
RUN echo 'export PATH="$PATH:/opt/nvim-linux64/bin"' >> ~/.bashrc
RUN echo 'export PATH="$PATH:/opt/nvim-linux64/bin"' >> ~/.zshrc

CMD ["ping", "google.com"]
