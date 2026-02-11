#include "stdafx.h"

#include "Frame.h"

#include <fstream>
#include <iomanip>

//-----------------------------------------------------------------------------
void Frame::SaveToFile(const std::string& filepath) const
{
	std::ofstream out(filepath);
	if (!out.is_open()) {
		std::cerr << "Failed to open debug file: " << filepath << std::endl;
		return;
	}

	out << "Frame TimeStamp: " << m_timeStamp << " ms\n\n";

	for (const auto& [jointName, transform] : m_pose) {
		out << "Joint: " << jointName << "\n";
		out << std::fixed << std::setprecision(4);

		const glm::mat4& mat = transform.getModelMatrix();
		for (int i = 0; i < 4; ++i) {
			out << "| ";
			for (int j = 0; j < 4; ++j) {
				out << std::setw(9) << mat[j][i] << " ";
			}
			out << "|\n";
		}

		out << "\n";
	}

	out.close();
	std::cout << "Frame saved to " << filepath << std::endl;
}