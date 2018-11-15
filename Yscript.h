#pragma once

namespace YScript
{
	class YScript
	{
	private:
		std::shared_ptr<YTML::DrawItemList> mDrawitem;
	public:
		YScript(std::shared_ptr<YTML::DrawItemList> ptr) : mDrawitem(ptr) {}
		

		bool runCode(std::wstring& code)
		{
			std::vector<std::wstring> commands;
			size_t cursor = 0, find;


			return true;
		}
	};
}