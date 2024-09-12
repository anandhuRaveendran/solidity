/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

#include <libyul/backends/evm/SSACFGTopologicalSort.h>

using namespace solidity::yul;

ForwardSSACFGTopologicalSort::ForwardSSACFGTopologicalSort(SSACFG const& _cfg):
	m_cfg(_cfg),
	m_explored(m_cfg.numBlocks(), false),
	m_preOrderPerBlock(m_cfg.numBlocks(), 0),
	m_maxSubtreePreOrder(m_cfg.numBlocks(), 0),
	m_predecessors(m_cfg.numBlocks())
{
	yulAssert(m_cfg.entry.value == 0);
	m_preOrder.reserve(m_cfg.numBlocks());
	m_postOrder.reserve(m_cfg.numBlocks());
	for (size_t id = 0; id < m_cfg.numBlocks(); ++id)
	{
		if (!m_explored[id])
			dfs(id);
	}

	for (auto const& [v1, v2]: m_potentialBackEdges)
		if (ancestor(v2, v1))
			m_backEdgeTargets.insert(v2);
}

void ForwardSSACFGTopologicalSort::dfs(size_t const _vertex) {
	yulAssert(!m_explored[_vertex]);
	m_explored[_vertex] = true;
	m_preOrderPerBlock[_vertex] = m_preOrder.size();
	m_maxSubtreePreOrder[_vertex] = m_preOrderPerBlock[_vertex];
	m_preOrder.push_back(_vertex);

	m_cfg.block(SSACFG::BlockId{_vertex}).forEachExit([&](SSACFG::BlockId const& _exitBlock){
		m_predecessors[_exitBlock.value].insert(_vertex);
		if (!m_explored[_exitBlock.value])
		{
			dfs(_exitBlock.value);
			m_maxSubtreePreOrder[_vertex] = std::max(m_maxSubtreePreOrder[_vertex], m_maxSubtreePreOrder[_exitBlock.value]);
		}
		else
			m_potentialBackEdges.emplace_back(_vertex, _exitBlock.value);
	});

	m_postOrder.push_back(_vertex);
}

std::vector<size_t> const& ForwardSSACFGTopologicalSort::preOrder() const { return m_preOrder; }
std::vector<size_t> const& ForwardSSACFGTopologicalSort::postOrder() const { return m_postOrder; }
std::vector<size_t> const& ForwardSSACFGTopologicalSort::maxSubtreePreOrder() const { return m_maxSubtreePreOrder; }
std::set<size_t> const& ForwardSSACFGTopologicalSort::backEdgeTargets() const { return m_backEdgeTargets; }
std::vector<std::set<size_t>> const& ForwardSSACFGTopologicalSort::predecessors() const { return m_predecessors; }
SSACFG const& ForwardSSACFGTopologicalSort::cfg() const { return m_cfg; }

bool ForwardSSACFGTopologicalSort::ancestor(size_t const _block1, size_t const _block2) const {
	yulAssert(_block1 < m_preOrderPerBlock.size());
	yulAssert(_block2 < m_preOrderPerBlock.size());

	auto const preOrderIndex1 = m_preOrderPerBlock[_block1];
	auto const preOrderIndex2 = m_preOrderPerBlock[_block2];

	bool const node1VisitedBeforeNode2 = preOrderIndex1 <= preOrderIndex2;
	bool const node2InSubtreeOfNode1 = preOrderIndex2 <= m_maxSubtreePreOrder[_block1];
	return node1VisitedBeforeNode2 && node2InSubtreeOfNode1;
}

bool ForwardSSACFGTopologicalSort::backEdge(SSACFG::BlockId const& _block1, SSACFG::BlockId const& _block2) const
{
	if (ancestor(_block2.value, _block1.value))
	{
		// check that block1 -> block2 is indeed an edge in the cfg
		bool isEdge = false;
		m_cfg.block(_block1).forEachExit([&_block2, &isEdge](SSACFG::BlockId const& _exit) { isEdge |= _block2 == _exit; });
		return isEdge;
	}
	return false;
}
