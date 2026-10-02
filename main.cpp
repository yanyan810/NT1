#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Novice.h>
#include <imgui.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

enum class MessageType : uint16_t {

	Text = 1,

};

// Windowsアプリのmain関数。ImGuiを使うため、Debugで実行する。
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	Novice::Initialize("NT1_01_02", 640, 360);
	char host[64] = "34.104.202.113";
	char port[8] = "8000"; 
	bool connected = false;

	char text[256] = "";
	char reply[512] = "";
	int sentBytesDisplay = 0;
	int receivedBytesDisplay = 0;

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

			// ==============================
			// 1. 送信フレームを作る
			// ==============================

			uint16_t bodyLength = static_cast<uint16_t>(std::strlen(text));

			uint16_t type = static_cast<uint16_t>(MessageType::Text);

			char sendFrame[512]{};

			// 位置0～1：本文の長さ
			std::memcpy(sendFrame, &bodyLength, sizeof(bodyLength));

			// 位置2～3：種別
			std::memcpy(sendFrame + 2, &type, sizeof(type));

			// 位置4～：本文
			std::memcpy(sendFrame + 4, text, bodyLength);

			// ヘッダー4バイト + 本文
			int frameSize = 4 + bodyLength;

			// ==============================
			// 2. フレームを全部送る
			// ==============================

			int sentBytes = 0;

			while (sentBytes < frameSize) {

				int n = send(clientSocket, sendFrame + sentBytes, frameSize - sentBytes, 0);

				if (n == SOCKET_ERROR) {

					connected = false;
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					break;
				}

				sentBytes += n;
			}

			sentBytesDisplay = sentBytes;

			// ==============================
			// 3. まず4バイトのヘッダーを受信
			// ==============================

			if (connected) {

				char receiveFrame[512]{};

				int receivedBytes = 0;

				while (receivedBytes < 4) {

					int n = recv(clientSocket, receiveFrame + receivedBytes, 4 - receivedBytes, 0);

					if (n == 0 || n == SOCKET_ERROR) {

						connected = false;
						closesocket(clientSocket);
						clientSocket = INVALID_SOCKET;
						break;
					}

					receivedBytes += n;
				}

				// ==============================
				// 4. ヘッダーを数値に戻す
				// ==============================

				if (connected) {

					uint16_t receivedLength = 0;
					uint16_t receivedType = 0;

					std::memcpy(&receivedLength, receiveFrame, sizeof(receivedLength));

					std::memcpy(&receivedType, receiveFrame + 2, sizeof(receivedType));

					// ==============================
					// 5. 本文を全部受信
					// ==============================

					int bodyReceived = 0;

					while (bodyReceived < receivedLength) {

						int n = recv(clientSocket, receiveFrame + 4 + bodyReceived, receivedLength - bodyReceived, 0);

						if (n == 0 || n == SOCKET_ERROR) {

							connected = false;
							closesocket(clientSocket);
							clientSocket = INVALID_SOCKET;
							break;
						}

						bodyReceived += n;
					}

					// ==============================
					// 6. 本文だけ文字列にする
					// ==============================

					if (connected && receivedType == 1) {

						receiveFrame[4 + bodyReceived] = '\0';

						std::memcpy(reply, receiveFrame + 4, bodyReceived + 1);
					}

					receivedBytesDisplay = 4 + bodyReceived;
				}
			}
		}

		if (ImGui::Button("Disconnect")) {
			// TODO 3: ソケットを閉じ、未接続の状態へ戻す。

			// 切断するとき
			closesocket(clientSocket);
			clientSocket = INVALID_SOCKET;
			connected = false;
		}

		ImGui::Text("Sent: %d bytes", sentBytesDisplay);
		ImGui::Text("Received: %d bytes", receivedBytesDisplay);
		ImGui::Text("Reply: %s", reply);

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
