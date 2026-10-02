#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Novice.h>
#include <imgui.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

// Windowsアプリのmain関数。ImGuiを使うため、Debugで実行する。
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	Novice::Initialize("NT1_01_01", 640, 360);
	char host[64] = "34.104.202.113";
	char port[8] = "8001"; // 01_00・01_01のEcho用ポート
	bool connected = false;

	char text[256] = " ";
	char receivedText[256] = " ";
	int totalSentBytes = 0;
	int totalReceivedBytes = 0;

	// TODO 1: 通信用の変数を用意し、Winsockを初期化する。
	WSADATA wsaData{};
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	SOCKET clientSocket = INVALID_SOCKET;

	while (Novice::ProcessMessage() == 0) {
		Novice::BeginFrame();
#ifdef USE_IMGUI
		ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(600, 200), ImGuiCond_Once);
		ImGui::Begin("TCP Connection");
		ImGui::BeginDisabled(connected);
		ImGui::InputText("Host", host, sizeof(host));
		ImGui::InputText("Port", port, sizeof(port));
		if (ImGui::Button("Connect")) {
			// TODO 2: 接続先を取得し、ソケットを作って接続する。
			addrinfo hints{};
			hints.ai_family = AF_INET;       // IPv4
			hints.ai_socktype = SOCK_STREAM; // TCP向け
			addrinfo* address = nullptr;
			if (getaddrinfo(host, port, &hints, &address) == 0) {
				// 成功：address　が接続先情報を指す

				// TCP用のソケット
				clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

				// OS同氏が3回やり取りして接続する
				connected = connect(clientSocket, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0;

				// 終わったら情報を返し、失敗をしたら閉じる
				freeaddrinfo(address);
				if (!connected) {

					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
				}
			}
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!connected);

		ImGui::InputText("Text", text, sizeof(text));
		if (ImGui::Button("Send")) {

			int dataSize = static_cast<int>(strlen(text));

			// 全部送信
			int sentBytes = 0;

			while (sentBytes < dataSize) {

				int n = send(clientSocket, text + sentBytes, dataSize - sentBytes, 0);

				if (n == SOCKET_ERROR) {

					connected = false;
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					break;
				}

				sentBytes += n;

			}

			totalSentBytes += sentBytes;

			//Echoを全部受信する
			if (connected) {
			
				int recevedBytes = 0;

				while (recevedBytes < dataSize) {
				
					int n = recv(
					    clientSocket, 
						receivedText + recevedBytes,
						dataSize - recevedBytes,
						0
					);
				
					if (n == 0 || n == SOCKET_ERROR) {
					
					connected = false;
						closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
						break;
					}

					recevedBytes += n;

				}
			
				totalReceivedBytes += recevedBytes;

				receivedText[recevedBytes] = '\0'; 

			
			}



		}

		if (ImGui::Button("Disconnect")) {
			// TODO 3: ソケットを閉じ、未接続の状態へ戻す。

			// 切断するとき
			closesocket(clientSocket);
			clientSocket = INVALID_SOCKET;
			connected = false;
		}

		ImGui::Text("Echo: %s", receivedText);
		ImGui::Text("Sent: %d bytes", totalSentBytes);
		ImGui::Text("Received: %d bytes", totalReceivedBytes);

		ImGui::EndDisabled();
		ImGui::Text("Status: %s", connected ? "Connected" : "Not connected");
		ImGui::End();
#endif
		Novice::EndFrame();
	}

	// TODO 4: 接続が残っていれば閉じ、Winsockを終了する。

	// 通信を使い終えるとき
	if (clientSocket != INVALID_SOCKET) {
		closesocket(clientSocket);
	}

	WSACleanup();

	Novice::Finalize();
	return 0;
}
