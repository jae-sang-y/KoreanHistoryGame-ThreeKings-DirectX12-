#pragma once

#define Float(x) std::stof(x)
#define Int(x) std::stoi(x)
#define Long(x) std::stoll(x)
#define Str(x) std::to_wstring(x)

namespace YTML
{
	class DrawItem
	{
	public:
		DrawItem() = default;
		DrawItem(std::wstring com)
		{
			std::wstring buf = com;

			if (buf.at(0) == '<' && buf.at(buf.length() - 1) == '>')
			{
				buf = buf.substr(1, buf.length() - 2);
				//os << buf << std::endl;

				Attribute[L"tag"] = buf.substr(0, buf.find(' '));
				//os << Attribute[L"tag"] << "|" << std::endl;

				buf = buf.substr(buf.find(' '));
				//os << buf << "|->" << std::endl;

				//Divided to Construct
				while(buf.size())
				{
					while (*buf.cbegin() == ' ')
						buf = buf.substr(1);

					size_t end = buf.find('"', buf.find('"') + 1);

					if (end == std::wstring::npos)
						break;

					
					std::wstring construct = buf.substr(0, end + 1);
					OutputDebugStringW((construct + L"\n").c_str());
					{
						size_t sign = construct.find('=');
						int a = 0;
						//Set RValue
						if (sign == std::wstring::npos)
						{
							Attribute[construct] = L"";
						}
						else
						{

							std::wstring rvalue = construct.substr(sign + 1);
							if (!(rvalue.at(0) == '"' && rvalue.at(rvalue.length() - 1) == '"'))
								rvalue = L"";
							else
								rvalue = rvalue.substr(1, rvalue.length() - 2);
							
							Attribute[construct.substr(0, sign)] = rvalue;
						}


					}

					buf = buf.substr(end + 1);
				}

			}
			else
			{
				//Corrupt
			}
			Attribute[L"color-r"] = L"0";
			Attribute[L"color-g"] = L"0";
			Attribute[L"color-b"] = L"0";

			Attribute[L"opacity"] = L"1";

			Attribute[L"left"] = L"0";
			Attribute[L"top"] = L"0";
			Attribute[L"width"] = L"64";
			Attribute[L"height"] = L"64";
		}
		std::unordered_map<std::wstring, std::wstring> Attribute;
	};

	using Args = std::unordered_map<std::wstring, std::wstring>;

	class DrawItemList 
	{
	public:
		std::list<std::unique_ptr<YTML::DrawItem>> data;


		/*void withClass(const std::wstring& className, std::initializer_list<std::wstring> args)
		{
			for (auto& O : data)
				if (O->Attribute[L"class"] == className)
					for (size_t i = 0; i < N / 2; ++i)
						O->Attribute[args[i * 2]] = args[i * 2 + 1];
		}*/

		void withID(const std::wstring& id, std::initializer_list<std::wstring> args)
		{
			std::wstring head;
			for (auto& O : data)
				if (O->Attribute[L"id"] == id)
				{
					for (auto P = args.begin();;)
					{
						if (P == args.end()) break;
						head = *(P++);
						if (P == args.end()) break;
						O->Attribute[head] = *(P++);
					}
					break;
				}
		}
	};

}