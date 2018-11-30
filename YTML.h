#pragma once

#define Float(x) std::stof(x)
#define Int(x) std::stoi(x)
#define Long(x) std::stoll(x)
#define Str(x) std::to_wstring(x)

namespace YTML
{
	std::vector<std::wstring> Split(const std::wstring& wstr)
	{
		std::vector<std::wstring> _Return;
		size_t offset = 0;
		size_t baricade = 0;

		while (offset != std::wstring::npos)
		{
			offset = wstr.find(L' ', offset + 1);
			_Return.push_back(wstr.substr(baricade, offset - baricade));
			baricade = offset + 1;
		}

		return _Return;
	}

	/*template<typename T>
	std::wstring operator+(std::wstring Tx, T Ty)
	{
		return Tx + std::to_wstring(Ty);
	}
	template<typename T>
	std::wstring operator+(T Tx, std::wstring Ty)
	{
		return std::to_wstring(Ty) + Tx;
	}*/

	class DrawItem
	{
	private:
		std::unordered_map<std::wstring, std::wstring> Attribute;
	public:
		std::unordered_set<std::wstring> Id, Class;
		float inherit_z_index = 0;
		float z_index = 0;
		bool background = false;
		bool border = false;
		bool enable = true;
		float background_color_r = 1;
		float background_color_g = 1;
		float background_color_b = 1;
		float color_r = 1;
		float color_g = 0;
		float color_b = 1;
		float left = 0;
		float top = 0;
		float inherit_left = 0;
		float inherit_top = 0;
		float width = 32;
		float height = 32;
		float opacity = 1;

		void SetAttribute(const std::wstring& Left, const std::wstring& Right)
		{
			if (Left == L"inherit-z-index")
			{
				inherit_z_index = std::stof(Right);
			}
			else if (Left == L"z-index")
			{
				z_index = std::stof(Right);
			}
			else if (Left == L"background-color-r")
			{
				background_color_r = std::stof(Right);
			}
			else if (Left == L"background-color-g")
			{
				background_color_g = std::stof(Right);
			}
			else if (Left == L"background-color-b")
			{
				background_color_b = std::stof(Right);
			}
			else if (Left == L"color-r")
			{
				color_r = std::stof(Right);
			}
			else if (Left == L"color-g")
			{
				color_g = std::stof(Right);
			}
			else if (Left == L"color-b")
			{
				color_b = std::stof(Right);
			}
			else if (Left == L"left")
			{
				left = std::stof(Right);
			}
			else if (Left == L"top")
			{
				top = std::stof(Right);
			}
			else if (Left == L"inherit-left")
			{
				inherit_left = std::stof(Right);
			}
			else if (Left == L"inherit-top")
			{
				inherit_top = std::stof(Right);
			}
			else if (Left == L"width")
			{
				width = std::stof(Right);
			}
			else if (Left == L"height")
			{
				height = std::stof(Right);
			}
			else if (Left == L"opacity")
			{
				opacity = std::stof(Right);
			}
			else if (Left == L"background")
			{
				background = Right != L"disable";
			}
			else if (Left == L"color-hex")
			{
				std::wstringstream st;
				st << std::hex << Right;
				unsigned int hex;
				st >> hex;
				st.clear();

				color_r = hex % 0x100 / 255.f;
				Attribute[L"color-r"] = Str(color_r);
				color_g = hex / 0x100 % 0x100 / 255.f;
				Attribute[L"color-g"] = Str(color_g);
				color_b = hex / 0x10000 % 0x100 / 255.f;
				Attribute[L"color-b"] = Str(color_b);
			}
			else if (Left == L"border")
			{
				border = Right != L"disable";
			}
			else if (Left == L"enable")
			{
				enable = Right != L"disable";
			}
			else if (Left == L"id")
			{
				Id.clear();
				for (const auto& S : Split(Right)) Id.insert(S);
			}
			else if (Left == L"class")
			{
				Class.clear();
				for (const auto& S : Split(Right)) Class.insert(S);
			}
		}

		DrawItem() = default;
		DrawItem(std::wstring com, const std::uint64_t& _uuid, const std::uint64_t& _parent = 0) : uuid(_uuid)
		{
			Attribute[L"inherit-z-index"] = L"0";
			Attribute[L"z-index"] = L"0";
			Attribute[L"color-r"] = L"1";
			Attribute[L"color-g"] = L"0";
			Attribute[L"color-b"] = L"1";
			Attribute[L"background_color-r"] = L"1";
			Attribute[L"background_color-g"] = L"1";
			Attribute[L"background_color-b"] = L"1";
			Attribute[L"left"] = L"0";
			Attribute[L"top"] = L"0";
			Attribute[L"inherit-left"] = L"0";
			Attribute[L"inherit-top"] = L"0";
			Attribute[L"width"] = L"32";
			Attribute[L"height"] = L"32";
			Attribute[L"opacity"] = L"1";
			Attribute[L"background"] = L"disable";
			Attribute[L"border"] = L"disable";
			Attribute[L"enable"] = L"enable";
			Attribute[L"id"] = L"";
			Attribute[L"class"] = L"";

			parent = _parent;

			std::wstring buf = com;

			if (buf.at(0) == '<' && buf.at(buf.length() - 1) == '>')
			{
				buf = buf.substr(1, buf.length() - 2);

				Attribute[L"tag"] = buf.substr(0, buf.find(' '));

				buf = buf.substr(buf.find(' '));

				//Divided to Construct
				while(buf.size())
				{
					while (*buf.cbegin() == ' ')
						buf = buf.substr(1);

					size_t end = buf.find('"', buf.find('"') + 1);

					if (end == std::wstring::npos)
						break;

					
					std::wstring construct = buf.substr(0, end + 1);
					{
						size_t sign = construct.find('=');
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
							SetAttribute(construct.substr(0, sign), rvalue);
						}


					}

					buf = buf.substr(end + 1);
				}

			}
			else
			{
				//Corrupt
			}

		}

		struct DrawAttribute
		{
			DrawItem* item;
			decltype(DrawItem::Attribute)::reference str;
			DrawAttribute(decltype(DrawItem::Attribute)::reference Str,DrawItem* Item) : str(Str), item(Item) {}
			DrawAttribute operator=(const std::wstring& ws)
			{
				str.second = ws;
				item->SetAttribute(str.first, ws);
				return *this;
			}
			bool operator==(const std::wstring& ws)
			{
				return str.second == ws;
			}
			bool operator!=(const std::wstring& ws)
			{
				return str.second != ws;
			}
			operator std::wstring() const {
				return str.second; 
			}
			const wchar_t* c_str()
			{
				return str.second.c_str();
			}
			size_t length()
			{
				return str.second.length();
			}
		};

		DrawAttribute operator[] (const std::wstring& index) {
			if (Attribute.find(index) == Attribute.end()) Attribute.insert(std::make_pair(index, L""));
			for (auto& A : Attribute) if (A.first == index) return DrawAttribute(A, this);
			throw;
		}


		const std::uint64_t uuid;
		std::uint64_t parent;
	};
	
	class DrawItemList 
	{
	private:
		std::uint64_t uuid_progress = 0;
	public:
		std::list<YTML::DrawItem> data;
		void Sort()
		{
			for (auto& O : data)
			{
				try
				{
					O.z_index = O.z_index + O.inherit_z_index;
				}
				catch (const std::exception&)
				{
					O.z_index = 0.f;
				}
			}
			
			data.sort([](YTML::DrawItem left, YTML::DrawItem right)
			{
				return left.z_index < right.z_index;
			});
		}


		class Query
		{
		public:
			std::list<std::list<YTML::DrawItem>::iterator> content;
			Query css(std::initializer_list<std::wstring> args)
			{
				std::wstring head;
				for (auto P = args.begin();;)
				{
					if (P == args.end()) break;
					head = *(P++);
					if (P == args.end()) break;
					for (auto& O : content)
					{
						O->SetAttribute(head, *(P));
						(*O)[head] = *(P++);
					}
				}

				return *this;
			}
			std::list<std::list<YTML::DrawItem>::iterator>::iterator begin() { return content.begin(); }
			std::list<std::list<YTML::DrawItem>::iterator>::iterator end() { return content.end(); }
			operator std::uint64_t() {
				
				return content.size() > 0 ? (*content.begin())->uuid : 0;
			}
		};
		enum class SelectorType
		{
			UUID,
			ID,
			CLASS
		};
		Query $(std::wstring wstr)
		{
			Query _Return;
			auto S = Split(wstr);

			std::list<std::list<YTML::DrawItem>::iterator> buffer;
			YTML::DrawItemList::SelectorType lastsec = YTML::DrawItemList::SelectorType::UUID;

			if (S.size() > 0)
			{
				if (S.at(0).at(0) == L'#')
				{
					lastsec = YTML::DrawItemList::SelectorType::ID;
					S.at(0) = S.at(0).substr(1);
				}
				else if (S.at(0).at(0) == L'.')
				{
					lastsec = YTML::DrawItemList::SelectorType::CLASS;
					S.at(0) = S.at(0).substr(1);
				}
				else if (S.at(0).at(0) == L'@')
				{
					lastsec = YTML::DrawItemList::SelectorType::UUID;
					S.at(0) = S.at(0).substr(1);
				}
				switch (lastsec)
				{
				case YTML::DrawItemList::SelectorType::UUID:
				{
					std::uint64_t uuid = std::stoull(S.at(0));
					for (auto O = data.begin(); O != data.end(); ++O)
						if (O->uuid == uuid)
						{
							_Return.content.push_back(O);
							break;
						}
				}
					break;
				case YTML::DrawItemList::SelectorType::ID:
					for (auto O = data.begin(); O != data.end(); ++O)
						if (O->Id.find(S.at(0)) != O->Id.end())
						{
							_Return.content.push_back(O);
							break;
						}
					break;
				case YTML::DrawItemList::SelectorType::CLASS:
					for (auto O = data.begin(); O != data.end(); ++O)
						if (O->Class.find(S.at(0)) != O->Class.end())
						{
							_Return.content.push_back(O);
						}
					break;
				}
				for (size_t i = 1; i < S.size(); ++i)
				{
					std::swap(buffer, _Return.content);
					_Return.content.clear();

					if (S.at(i).at(0) == L'#')
					{
						lastsec = YTML::DrawItemList::SelectorType::ID;
						S.at(i) = S.at(i).substr(1);
					}
					else if (S.at(i).at(0) == L'.')
					{
						lastsec = YTML::DrawItemList::SelectorType::CLASS;
						S.at(i) = S.at(i).substr(1);
					}
					else if (S.at(i).at(0) == L'@')
					{
						lastsec = YTML::DrawItemList::SelectorType::UUID;
						S.at(i) = S.at(i).substr(1);
					}

					bool hard_break = true;
					switch (lastsec)
					{
					case YTML::DrawItemList::SelectorType::UUID:
					{
						std::uint64_t uuid = std::stoull(S.at(i));
						for (auto O = data.begin(); O != data.end() && hard_break; ++O)
							if (O->uuid == uuid && O->parent != 0)
							{
								for (auto P : buffer)
								{
									if (P->uuid == O->parent)
									{
										_Return.content.push_back(O);
										break;
									}
								}
								break;
							}
					}
						break;
					case YTML::DrawItemList::SelectorType::ID:
						for (auto O = data.begin(); O != data.end() && hard_break; ++O)
							if (O->Id.find(S.at(i)) != O->Id.end() && O->parent != 0)
							{
								for (auto P : buffer)
								{
									if (P->uuid == O->parent)
									{
										_Return.content.push_back(O);
										hard_break = false;
										break;
									}
								}
							}
						break;
					case YTML::DrawItemList::SelectorType::CLASS:
						if (S.at(i) == L".")
						{
							for (auto P : buffer)
							{
								hard_break = true;
								if (P->parent > 0)
									for (auto O : withUUID(P->parent))
									{
										_Return.content.push_back(O);
										hard_break = false;
									}
								if (hard_break)
									_Return.content.push_back(P);
							}
						}
						else
						{
							for (auto O = data.begin(); O != data.end() && hard_break; ++O)
								if (O->Class.find(S.at(i)) != O->Class.end() && O->parent != 0)
								{
									for (auto P : buffer)
									{
										if (P->uuid == O->parent)
										{
											_Return.content.push_back(O);
											hard_break = false;
											break;
										}
									}
								}
						}
						break;
					}
				}
			}



			return _Return;
		}

		Query Insert(wchar_t* tag, const std::uint64_t& parent = 0)
		{
			Query _Return;
			data.push_back(YTML::DrawItem(tag, ++uuid_progress, parent));
			_Return.content.push_back((++data.rbegin()).base());
			Sort();
			return _Return;
		}

		Query withUUID(const std::uint64_t& uuid)
		{
			Query _Return;
			for (auto O = data.begin(); O != data.end(); ++O)
				if (O->uuid == uuid)
				{
					_Return.content.push_back(O);
					break;
				}
			return _Return;
		}
		Query withUUID(const std::uint64_t& uuid, std::initializer_list<std::wstring> args)
		{
			Query _Return;
			std::wstring head;
			for (auto O = data.begin(); O != data.end(); ++O)
				if (O->uuid == uuid)
				{
					for (auto P = args.begin();;)
					{
						if (P == args.end()) break;
						head = *(P++);
						if (P == args.end()) break;
						(*O)[head] = *(P++);
					}
					_Return.content.push_back(O);
					break;
				}
			return _Return;
		}
		
	};

}